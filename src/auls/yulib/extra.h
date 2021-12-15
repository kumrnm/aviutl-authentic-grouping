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
RewriteFunction用ユーティリティ。以下の3要素を記述します。
- 書き換え前関数退避用変数（*_original）
- 操作する関数の型（t_*）
- 書き換え後関数の宣言部（*_hooked）

使い方: HOOKED(return_type, modifiers, name, ...args) { (hooked code); } 
*/
#define HOOKED(return_type, modifiers, name, ...) \
return_type (modifiers *name##_original)(__VA_ARGS__);\
typedef decltype(name##_original) t_##name;\
return_type modifiers name##_hooked(__VA_ARGS__)