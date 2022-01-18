#include <Windows.h>
#include <cstring>
#include "win32_function_injection.h"

namespace win32_function_injection {

	const unsigned char machine_code_template[] = {
		0x89, 0x05, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV dword ptr [&register_buffer.EAX], EAX
		0x89, 0x0d, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV dword ptr [&register_buffer.ECX], ECX
		0x89, 0x15, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV dword ptr [&register_buffer.EDX], EDX
		0x89, 0x1d, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV dword ptr [&register_buffer.EBX], EBX
		0x89, 0x25, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV dword ptr [&register_buffer.ESP], ESP
		0x89, 0x2d, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV dword ptr [&register_buffer.EBP], EBP
		0x89, 0x35, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV dword ptr [&register_buffer.ESI], ESI
		0x89, 0x3d, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV dword ptr [&register_buffer.EDI], EDI

		0x68, 0xFF, 0xFF, 0xFF, 0xFF,        // PUSH register_buffer
		0xe8, 0xFF, 0xFF, 0xFF, 0xFF,        // CALL func
		0x83, 0xc4, 0x04,                    // ADD esp, 4
		0x89, 0x05, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV dword ptr [&return_position_buffer], EAX

		0x8b, 0x05, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV EAX, dword ptr [&register_buffer.EAX]
		0x8b, 0x0d, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV ECX, dword ptr [&register_buffer.ECX]
		0x8b, 0x15, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV EDX, dword ptr [&register_buffer.EDX]
		0x8b, 0x1d, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV EBX, dword ptr [&register_buffer.EBX]
		0x8b, 0x25, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV ESP, dword ptr [&register_buffer.ESP]
		0x8b, 0x2d, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV EBP, dword ptr [&register_buffer.EBP]
		0x8b, 0x35, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV ESI, dword ptr [&register_buffer.ESI]
		0x8b, 0x3d, 0xFF, 0xFF, 0xFF, 0xFF,  // MOV EDI, dword ptr [&register_buffer.EDI]

		0xff, 0x25, 0xFF, 0xFF, 0xFF, 0xFF,  // JMP dword ptr [&return_position_buffer]
	};

	const FUNCTION_INJECTION_INFO inject_function(void* address, void* (*func)(REGISTER& reg)) {
		FUNCTION_INJECTION_INFO info;

		// バッファ領域を確保
		unsigned char* machine_code = (unsigned char*)VirtualAlloc(
			NULL,
			sizeof(machine_code_template),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_EXECUTE_READWRITE
		);
		REGISTER* register_buffer_ptr = (REGISTER*)VirtualAlloc(
			NULL,
			sizeof(REGISTER),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_EXECUTE_READWRITE
		);
		void** return_position_buffer_ptr = (void**)VirtualAlloc(
			NULL,
			sizeof(void*),
			MEM_COMMIT | MEM_RESERVE,
			PAGE_EXECUTE_READWRITE
		);

		if (machine_code == nullptr || register_buffer_ptr == nullptr || return_position_buffer_ptr == nullptr) {
			return info;
		}

		// 機械語コードを生成
		std::memcpy(machine_code, machine_code_template, sizeof(machine_code_template));
		*(unsigned int**)(&machine_code[2]) = *(unsigned int**)(&machine_code[69]) = &register_buffer_ptr->EAX;
		*(unsigned int**)(&machine_code[8]) = *(unsigned int**)(&machine_code[75]) = &register_buffer_ptr->ECX;
		*(unsigned int**)(&machine_code[14]) = *(unsigned int**)(&machine_code[81]) = &register_buffer_ptr->EDX;
		*(unsigned int**)(&machine_code[20]) = *(unsigned int**)(&machine_code[87]) = &register_buffer_ptr->EBX;
		*(unsigned int**)(&machine_code[26]) = *(unsigned int**)(&machine_code[93]) = &register_buffer_ptr->ESP;
		*(unsigned int**)(&machine_code[32]) = *(unsigned int**)(&machine_code[99]) = &register_buffer_ptr->EBP;
		*(unsigned int**)(&machine_code[38]) = *(unsigned int**)(&machine_code[105]) = &register_buffer_ptr->ESI;
		*(unsigned int**)(&machine_code[44]) = *(unsigned int**)(&machine_code[111]) = &register_buffer_ptr->EDI;
		*(REGISTER**)(&machine_code[49]) = register_buffer_ptr;
		*(void**)(&machine_code[54]) = (void*)((unsigned int)func - ((unsigned int)&machine_code[53] + 5));
		*(void**)(&machine_code[63]) = return_position_buffer_ptr;
		*(void**)(&machine_code[117]) = return_position_buffer_ptr;

		// JMP命令を挿入
		DWORD oldProtect;
		if (!VirtualProtect(address, 5, PAGE_READWRITE, &oldProtect)) return info;
		*(unsigned char*)(address) = 0xe9;
		*(unsigned int*)((unsigned int)address + 1) = (unsigned int)machine_code - ((unsigned int)address + 5);
		VirtualProtect(address, 5, oldProtect, &oldProtect);

		info.succeed = true;
		info.machine_code_ptr = machine_code;
		info.register_buffer_ptr = register_buffer_ptr;
		info.return_position_buffer_ptr = return_position_buffer_ptr;
		return info;
	}
}