#include <Windows.h>
#include "tchar.h"
#include "aviutl_plugin_sdk/filter.h"
#include "auls/memref.h"
#include "auls/yulib/extra.h"
#include "proc.h"
#include "gui.h"

#ifdef _DEBUG
#include "dbgstream/dbgstream.h"
#endif


#define PLUGIN_NAME TEXT("真・グループ制御")
#ifdef _DEBUG
#define PLUGIN_NAME_SUFFIX TEXT("（Debug）")
#else
#define PLUGIN_NAME_SUFFIX TEXT("")
#endif
#define PLUGIN_VERSION TEXT("1.1.0")


FILTER* g_fp = nullptr;

void show_error(LPCTSTR text) {
	MessageBox(NULL, text, PLUGIN_NAME, MB_OK | MB_ICONERROR);
}


//================================
//  デバッグ用（リリース時無効）
//================================

#ifdef _DEBUG

void _DEBUG_FUNC() {
	proc::debug();
	gui::debug();
	cdbg << std::flush;
}

#endif


//================================
//  イベント処理
//================================

HOOKED(BOOL, , exedit_WndProc, HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void* editp, void* fp) {
	// 真・グループ制御の有効/無効が変更された際、拡張編集のタイムラインを再描画する
	if (g_fp != nullptr) {
		static bool pre_active = false;
		const bool active = g_fp->exfunc->is_filter_active(g_fp);
		if (active != pre_active) gui::rerender_timeline();
		pre_active = active;
	}

#ifdef _DEBUG
	// F5キーでデバッグ動作
	if ((message == WM_KEYUP || message == WM_FILTER_MAIN_KEY_UP) && wparam == VK_F5) {
		_DEBUG_FUNC();
		return FALSE;
	}
#endif

	return exedit_WndProc_original(hwnd, message, wparam, lparam, editp, fp);
}


//================================
//  初期化処理
//================================

BOOL func_init(FILTER* fp) {
	g_fp = fp;
	auto exedit = auls::Exedit_GetFilter(fp);
	if (!exedit) {
		show_error(TEXT("拡張編集が見つかりませんでした。"));
	}
	else if (_tcscmp(exedit->information, TEXT("拡張編集(exedit) version 0.92 by ＫＥＮくん")) != 0) {
		show_error(TEXT("このバージョンの拡張編集には対応していません。"));
	}
	else {
		auls::Memref_Init((FILTER*)fp);
		proc::init(fp, exedit);
		gui::init(fp, exedit);

		exedit_WndProc_original = exedit->func_WndProc;
		exedit->func_WndProc = exedit_WndProc_hooked;
	}

	return TRUE;
}


//================================
//  エクスポート情報
//================================

FILTER_DLL filter = {
	.flag = FILTER_FLAG_NO_CONFIG | FILTER_FLAG_EX_INFORMATION,
	.name = (TCHAR*)PLUGIN_NAME,
	.func_init = func_init,
	.information = (TCHAR*)(PLUGIN_NAME PLUGIN_NAME_SUFFIX TEXT(" v") PLUGIN_VERSION)
};

EXTERN_C __declspec(dllexport) FILTER_DLL* __stdcall GetFilterTable(void) {
	return &filter;
}