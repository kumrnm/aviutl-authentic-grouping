#include "aviutl_plugin_sdk/filter.h"
#include "auls/yulib/extra.h"
#include "util.h"

#ifdef _DEBUG
#include "dbgstream/dbgstream.h"
#endif


namespace gui {

	FILTER* g_exedit = nullptr;
	DWORD exedit_base = NULL;


	//================================
	//  �g���ҏW�̓��������ւ̊���
	//================================

	BYTE* replacement_address = NULL;
	const int replacement_size = 7;
	BYTE initial_code[replacement_size];
	BYTE replaced_code[replacement_size] = {
		// injection_code�����s
		0xe9, 0x00, 0x00, 0x00, 0x00, // JMP�i�W�����v��͎��s���ɏ���������j
		0x90, 0x90 // NOP
	};

	// �g���ҏW��0x3854e�Ɋ��荞�ރR�[�h
	BYTE injection_code[] = {
		// EAX�͏I�[�r�b�g�}�X�N�ie.g. �O���[�v����̏I�[�ł̓O���[�v����ɑΉ�����r�b�g��0�ɂȂ��Ă���j
		0xb8, 0xff, 0xff, 0x00, 0x00, // EAX <- 0x0000ffff
		0x2b, 0xc2, // EAX <- EAX - EDX

		// �O���[�v����̏I�[�t���O���폜
		0x0d, 0x02, 0x00, 0x00, 0x00, // EAX <- EAX | 0x00000002

		// �߂�
		0xe9, 0x00, 0x00, 0x00, 0x00 // JMP�i�W�����v��͎��s���ɏ���������j
	};

	bool set_gui_replacement_enabled(bool value) {
		if (replacement_address == NULL) return false;

		DWORD dwOldProtect;
		VirtualProtect(replacement_address, replacement_size, PAGE_READWRITE, &dwOldProtect);

		BYTE* data = value ? replaced_code : initial_code;
		for (int i = 0; i < replacement_size; ++i) replacement_address[i] = data[i];

		VirtualProtect(replacement_address, replacement_size, dwOldProtect, &dwOldProtect);

		return true;
	}

	bool rerender_timeline() {
		if (g_exedit == nullptr || g_exedit->hwnd == nullptr) return false;
		typedef void (*t_exedit_rendering_func)(HWND hwnd);
		((t_exedit_rendering_func)(exedit_base + 0x387f0))(g_exedit->hwnd);
		return true;
	}


	//================================
	//  ����������
	//================================

	void init(FILTER* fp, FILTER* exedit) {
		g_exedit = exedit;
		exedit_base = (DWORD)exedit->dll_hinst;
		cdbg << (void*)exedit_base << std::endl;

		// �@�B�ꏑ�������̂��߂̏���

		replacement_address = (BYTE*)(exedit_base + 0x3854e);
		for (int i = 0; i < replacement_size; ++i) initial_code[i] = replacement_address[i];

		// JMP���߂̃W�����v���ݒ�
		*(DWORD*)(replaced_code + 1) = (DWORD)injection_code - ((DWORD)replacement_address + 5);
		*(DWORD*)(injection_code + sizeof(injection_code) - 4) = (DWORD)replacement_address + replacement_size - ((DWORD)injection_code + sizeof(injection_code));

		// injection_code���@�B��Ƃ��Ď��s�\�ɂ���
		DWORD dwOldProtect;
		VirtualProtect(injection_code, sizeof(injection_code), PAGE_EXECUTE_READWRITE, &dwOldProtect);
	}


	//================================
	//  �f�o�b�O�p�i�����[�X�������j
	//================================

#ifdef _DEBUG

	void debug() {
		cdbg << std::flush;
	}

#endif

}