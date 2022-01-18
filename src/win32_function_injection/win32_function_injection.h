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

	// アドレス address に func の実行命令を挿入します。
	// func 内では引数 reg を通してレジスタの読み書きができます。
	// func 実行後に復帰するアドレスを func の戻り値で指定します。
	//（ address ～ address+4 の領域は CALL 命令で上書きされるため復帰できません。）
	const FUNCTION_INJECTION_INFO inject_function(void* address, void* (*func)(REGISTER& reg));
}