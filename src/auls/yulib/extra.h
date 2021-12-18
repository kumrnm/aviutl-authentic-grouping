////////////////////////////////////////////////////////////////////////////////
// yulib/extra.h
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef YULIB_EXTRA_H
#define YULIB_EXTRA_H

#include <stdio.h>
#include <windows.h>
#include <imagehlp.h>

#pragma comment(lib, "imagehlp.lib")

namespace yulib {

	void* RewriteFunction(LPCSTR modname, LPCSTR funcname, void* funcptr);

} // namespace yulib

#endif // #ifndef YULIB_EXTRA_H


/*
RewriteFunction�p���[�e�B���e�B�B�ȉ���3�v�f���L�q���܂��B
- ���������O�֐��ޔ�p�ϐ��i*_original�j
- ���삷��֐��̌^�it_*�j
- ����������֐��̐錾���i*_hooked�j

�g����: HOOKED(return_type, modifiers, name, ...args) { (hooked code); } 
*/
#define HOOKED(return_type, modifiers, name, ...) \
return_type (modifiers *name##_original)(__VA_ARGS__);\
typedef decltype(name##_original) t_##name;\
return_type modifiers name##_hooked(__VA_ARGS__)