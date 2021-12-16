#pragma once

#include "aviutl_plugin_sdk/filter.h"

void proc_init(FILTER* fp, FILTER* exedit);

#ifdef _DEBUG
void proc_debug();
#endif