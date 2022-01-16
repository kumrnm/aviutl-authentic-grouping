#pragma once

#include "aviutl_plugin_sdk/filter.h"

namespace gui {

	void init(FILTER* fp, FILTER* exedit);
	bool set_gui_replacement_enabled(bool value);
	bool rerender_timeline();

#ifdef _DEBUG
	void debug();
#endif

}