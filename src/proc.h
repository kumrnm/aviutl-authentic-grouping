#pragma once

#include "aviutl_plugin_sdk/filter.h"

BOOL proc(void* fp, FILTER_PROC_INFO* fpip, BOOL(*exedit_proc_original)(void* fp, FILTER_PROC_INFO* fpip));

#ifdef _DEBUG
void proc_debug();
#endif