#pragma once

#include <windows.h>
#include <cstdlib>
#include <iostream>

template <class Ch,class Tr=std::char_traits<Ch> >
class basic_debug_streambuf : public std::basic_streambuf<Ch,Tr> {
private:
  static const int init_size=0x100;
  int current_size;
  Ch* buffer;

public:
  basic_debug_streambuf() :current_size(init_size), buffer(NULL) {
    buffer = static_cast<Ch*>(std::malloc(sizeof(Ch)*init_size));
    if(buffer==NULL) throw std::bad_alloc();
    this->setp(buffer, &buffer[init_size]);
  }

  ~basic_debug_streambuf() {
    sync();
    free(buffer);
  }
  
protected:
  std::basic_streambuf<Ch,Tr>* setbuf(Ch* b,int s) {
    std::free(buffer);
    this->setp(b, &b[s]);
    return this;
  }

  int overflow(int ch = Tr::eof()) {
    if(!Tr::eq_int_type(ch, Tr::eof())) {
      if(this->pptr() >= &buffer[current_size]) {
        int old_size = current_size;
        current_size*=2;
        Ch* old_buffer=buffer;
        buffer=static_cast<Ch*>(realloc(buffer, sizeof(Ch)*current_size));
        if(buffer==NULL) {
          free(old_buffer);
          throw std::bad_alloc();
        }
        this->setp(buffer, &buffer[current_size]);
        this->pbump(old_size);
      }
      *(this->pptr()) = Tr::to_char_type(ch);
      this->pbump(1);
      return Tr::not_eof(ch);
    } else {
      return Tr::eof();
    }
  }

  int sync(void) {
    overflow('\0');
    OutputDebugString(this->pbase());
    this->setp(buffer, &buffer[current_size]);
    return 0;
  }
};

template <class Ch,class Tr=std::char_traits<Ch> >
class basic_debug_stream : public std::basic_ostream<Ch,Tr> {
public:
  basic_debug_stream(void)
    :std::basic_ostream<Ch,Tr>(new basic_debug_streambuf<Ch,Tr>) {
  }

  ~basic_debug_stream(void) {
    this->flush();
    delete this->rdbuf();
  }
};

typedef basic_debug_streambuf<char> debug_streambuf;
typedef basic_debug_stream<char> debug_stream;

extern debug_stream cdbg;