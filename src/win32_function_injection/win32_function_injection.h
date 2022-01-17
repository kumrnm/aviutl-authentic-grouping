#pragma once

namespace win32_function_injection {
	struct REGISTER {
		unsigned int EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI;
	};

	// アドレス address に func の実行命令を挿入します。
	// 戻り値は、生成した機械語コードへのポインタです。
	//     - funcの戻り値: func 実行後に復帰する機械語コードのアドレス
	void* inject_function(void* address, void* (*func)(REGISTER& reg));
}