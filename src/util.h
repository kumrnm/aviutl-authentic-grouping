// —v auls::Memref_Init

#pragma once

#include "windows.h"
#include "auls/aulslib/exedit.h"

namespace util {

	const int LAYER_COUNT = 100;

	auls::EXEDIT_OBJECT* get_object_by_id(const int id);
	auls::EXEDIT_OBJECT* midpoint_leader(auls::EXEDIT_OBJECT* obj);
	bool is_group_object(auls::EXEDIT_OBJECT* obj);
	int get_group_control_range(auls::EXEDIT_OBJECT* group_obj);
	bool group_can_contain(auls::EXEDIT_OBJECT* group_obj, auls::EXEDIT_OBJECT* target_obj);

	DWORD rewrite_call_target(const DWORD call_operation_address, const void* target_function);

}