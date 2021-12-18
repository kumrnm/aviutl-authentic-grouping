#include <Windows.h>
#include "tchar.h"
#include "aviutl_plugin_sdk/filter.h"
#include "auls/memref.h"
#include "auls/yulib/extra.h"
#include "proc.h"

#ifdef _DEBUG
#include "dbgstream/dbgstream.h"
#endif


#ifdef _DEBUG
#define PLUGIN_NAME TEXT("�^�E�O���[�v����iDebug�j")
#else
#define PLUGIN_NAME TEXT("�^�E�O���[�v����")
#endif
#define PLUGIN_VERSION TEXT("1.0.0")


void show_error(LPCTSTR text) {
	MessageBox(NULL, text, PLUGIN_NAME, MB_OK | MB_ICONERROR);
}


//================================
//  �f�o�b�O�p�i�����[�X�������j
//================================

#ifdef _DEBUG

void _DEBUG_FUNC() {
	proc::debug();
	cdbg << std::flush;
}

HOOKED(BOOL, , exedit_WndProc, HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void* editp, void* fp) {
	// F5�L�[�Ńf�o�b�O����
	if ((message == WM_KEYUP || message == WM_FILTER_MAIN_KEY_UP) && wparam == VK_F5) {
		_DEBUG_FUNC();
		return FALSE;
	}

	return exedit_WndProc_original(hwnd, message, wparam, lparam, editp, fp);
}

#endif


//================================
//  ����������
//================================

BOOL func_init(FILTER* fp) {
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

#ifdef _DEBUG
		exedit_WndProc_original = exedit->func_WndProc;
		exedit->func_WndProc = exedit_WndProc_hooked;
#endif
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
	.information = (TCHAR*)(PLUGIN_NAME TEXT(" v") PLUGIN_VERSION)
};

EXTERN_C __declspec(dllexport) FILTER_DLL* __stdcall GetFilterTable(void) {
	return &filter;
}