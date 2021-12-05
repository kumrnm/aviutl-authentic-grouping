#include "stdafx.h"
#include "common.h"
#include "filter.h"


//================================
//  初期化処理
//================================

BOOL func_init(FILTER* fp) {
	return TRUE;
}


//================================
//  エクスポート情報
//================================

FILTER_DLL filter = {
	.flag = FILTER_FLAG_ALWAYS_ACTIVE | FILTER_FLAG_NO_CONFIG,
	.name = (TCHAR*)PLUGIN_NAME,
	.func_init = func_init,
};

EXTERN_C __declspec(dllexport) FILTER_DLL* __stdcall GetFilterTable(void) {
	return &filter;
}