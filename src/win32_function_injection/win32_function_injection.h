#pragma once

namespace win32_function_injection {
	struct REGISTER {
		unsigned int EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI;
	};

	// �A�h���X address �� func �̎��s���߂�}�����܂��B
	// �߂�l�́A���������@�B��R�[�h�ւ̃|�C���^�ł��B
	//     - func�̖߂�l: func ���s��ɕ��A����@�B��R�[�h�̃A�h���X
	void* inject_function(void* address, void* (*func)(REGISTER& reg));
}