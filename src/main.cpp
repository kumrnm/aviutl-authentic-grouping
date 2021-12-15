#include <Windows.h>
#include "tchar.h"
#include "aviutl_plugin_sdk/filter.h"
#include "auls/memref.h"
#include "auls/yulib/extra.h"
#include "proc.h"


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

#include "dbgstream/dbgstream.h"

void _DEBUG_INIT(FILTER* exedit) {
}

void _DEBUG_FUNC() {
	cdbg << std::flush;
}

#endif


//================================
//  �g���ҏW�̃C�x���g�n���h��
//================================

// �t�B���^�i�`��j����

HOOKED(BOOL, , exedit_proc, void* fp, FILTER_PROC_INFO* fpip) {
	return proc(fp, fpip, exedit_proc_original);
}

// �E�B���h�E���b�Z�[�W����

HOOKED(BOOL, , exedit_WndProc, HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void* editp, void* fp) {
	// �f�o�b�O�R�}���h�i�����[�X�������j
#ifdef _DEBUG
	if ((message == WM_KEYUP || message == WM_FILTER_MAIN_KEY_UP) && wparam == VK_F5) {
		_DEBUG_FUNC();
		return FALSE;
	}
#endif

	auto res = exedit_WndProc_original(hwnd, message, wparam, lparam, editp, fp);
	return res;
}


//================================
//  ����������
//================================

BOOL func_init(FILTER* fp) {
	// �g���ҏW�v���O�C���̃C�x���g�n���h���������ւ���
	auto exedit = auls::Exedit_GetFilter(fp);
	if (!exedit) {
		show_error(TEXT("�g���ҏW��������܂���ł����B"));
	}
	else if (_tcscmp(exedit->information, TEXT("�g���ҏW(exedit) version 0.92 by �j�d�m����")) != 0) {
		show_error(TEXT("���̃o�[�W�����̊g���ҏW�ɂ͑Ή����Ă��܂���B"));
	}
	else {
		auls::Memref_Init((FILTER*)fp);

		exedit_proc_original = exedit->func_proc;
		exedit->func_proc = exedit_proc_hooked;

#ifdef _DEBUG
		exedit_WndProc_original = exedit->func_WndProc;
		exedit->func_WndProc = exedit_WndProc_hooked;
		_DEBUG_INIT(exedit);
#endif
	}

	return TRUE;
}


//================================
//  �G�N�X�|�[�g���
//================================

FILTER_DLL filter = {
	.flag = FILTER_FLAG_ALWAYS_ACTIVE | FILTER_FLAG_NO_CONFIG | FILTER_FLAG_EX_INFORMATION,
	.name = (TCHAR*)PLUGIN_NAME,
	.func_init = func_init,
	.information = (TCHAR*)(PLUGIN_NAME TEXT(" v") PLUGIN_VERSION)
};

EXTERN_C __declspec(dllexport) FILTER_DLL* __stdcall GetFilterTable(void) {
	return &filter;
}