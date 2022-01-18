#pragma once

namespace win32_function_injection {
	struct REGISTER {
		unsigned int EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI;
	};

	struct FUNCTION_INJECTION_INFO {
		bool succeed = false;
		void* machine_code_ptr = nullptr;
		REGISTER* register_buffer_ptr = nullptr;
		void** return_position_buffer_ptr = nullptr;
	};

	// �A�h���X address �� func �̎��s���߂�}�����܂��B
	// func ���ł͈��� reg ��ʂ��ă��W�X�^�̓ǂݏ������ł��܂��B
	// func ���s��ɕ��A����A�h���X�� func �̖߂�l�Ŏw�肵�܂��B
	//�i address �` address+4 �̗̈�� CALL ���߂ŏ㏑������邽�ߕ��A�ł��܂���B�j
	const FUNCTION_INJECTION_INFO inject_function(void* address, void* (*func)(REGISTER& reg));
}