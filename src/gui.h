#pragma once

#include "aviutl_plugin_sdk/filter.h"

namespace gui {

	void init(FILTER* exedit);
	bool rerender_timeline();

#ifdef _DEBUG
	void debug();
#endif

}