#pragma once

#include "windows.h"
#include "auls/aulslib/exedit.h"

namespace util {

	const int LAYER_COUNT = 100;

	void init(FILTER* fp);
	bool is_filter_active();

	auls::EXEDIT_OBJECT* get_object_by_id(const int id);
	auls::EXEDIT_OBJECT* midpoint_leader(auls::EXEDIT_OBJECT* obj);
	bool is_group_object(auls::EXEDIT_OBJECT* obj);
	int get_group_control_range(auls::EXEDIT_OBJECT* group_obj);
	bool group_can_contain(auls::EXEDIT_OBJECT* group_obj, auls::EXEDIT_OBJECT* target_obj);

	DWORD rewrite_call_target(const DWORD call_operation_address, const void* target_function);

}

/*
util::rewrite_call_target 用ユーティリティ。以下の3要素を記述します。
- 書き換え前関数退避用変数（*_original）
- 操作する関数の型（t_*）
- 書き換え後関数の宣言部（*_hooked）

使い方: HOOKED(return_type, modifiers, name, ...args) { (hooked code); }
*/
#define HOOKED(return_type, modifiers, name, ...) \
return_type (modifiers *name##_original)(__VA_ARGS__);\
typedef decltype(name##_original) t_##name;\
return_type modifiers name##_hooked(__VA_ARGS__)