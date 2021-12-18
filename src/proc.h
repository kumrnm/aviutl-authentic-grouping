#pragma once

#include "aviutl_plugin_sdk/filter.h"

namespace proc{

	void init(FILTER* fp, FILTER* exedit);

#ifdef _DEBUG
	void debug();
#endif

}