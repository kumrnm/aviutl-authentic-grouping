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


#define PLUGIN_NAME TEXT("�^�E�O���[�v����")
#ifdef _DEBUG
#define PLUGIN_NAME_SUFFIX TEXT("�iDebug�j")
#else
#define PLUGIN_NAME_SUFFIX TEXT("")
#endif
#define PLUGIN_VERSION TEXT("1.1.0")


FILTER* g_fp = nullptr;

void show_error(LPCTSTR text) {
	MessageBox(NULL, text, PLUGIN_NAME, MB_OK | MB_ICONERROR);
}


//================================
//  �f�o�b�O�p�i�����[�X�������j
//================================

#ifdef _DEBUG

void _DEBUG_FUNC() {
	proc::debug();
	gui::debug();
	cdbg << std::flush;
}

#endif


//================================
//  �C�x���g����
//================================

HOOKED(BOOL, , exedit_WndProc, HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void* editp, void* fp) {
	// �^�E�O���[�v����̗L��/�������ύX���ꂽ�ہA�g���ҏW�̃^�C�����C�����ĕ`�悷��
	if (g_fp != nullptr) {
		static bool pre_active = false;
		const bool active = g_fp->exfunc->is_filter_active(g_fp);
		if (active != pre_active) gui::rerender_timeline();
		pre_active = active;
	}

#ifdef _DEBUG
	// F5�L�[�Ńf�o�b�O����
	if ((message == WM_KEYUP || message == WM_FILTER_MAIN_KEY_UP) && wparam == VK_F5) {
		_DEBUG_FUNC();
		return FALSE;
	}
#endif

	return exedit_WndProc_original(hwnd, message, wparam, lparam, editp, fp);
}


//================================
//  ����������
//================================

BOOL func_init(FILTER* fp) {
	g_fp = fp;
	auto exedit = auls::Exedit_GetFilter(fp);
	if (!exedit) {
		show_error(TEXT("�g���ҏW��������܂���ł����B"));
	}
	else if (_tcscmp(exedit->information, TEXT("�g���ҏW(exedit) version 0.92 by �j�d�m����")) != 0) {
		show_error(TEXT("���̃o�[�W�����̊g���ҏW�ɂ͑Ή����Ă��܂���B"));
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
//  �G�N�X�|�[�g���
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