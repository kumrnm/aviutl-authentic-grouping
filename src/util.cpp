#include "windows.h"
#include "auls/memref.h"
#include "util.h"


namespace util {

	auls::EXEDIT_OBJECT* get_object_by_id(const int id) {
		return *auls::Exedit_ObjectTable() + id;
	}

	auls::EXEDIT_OBJECT* midpoint_leader(auls::EXEDIT_OBJECT* obj) {
		return obj->index_midpt_leader < 0 ? obj : get_object_by_id(obj->index_midpt_leader);
	}

	bool is_group_object(auls::EXEDIT_OBJECT* obj) {
		return obj->type == 28;
	}

	// グループの制御レイヤー数を取得する
	int get_group_control_range(auls::EXEDIT_OBJECT* group_obj) {
		group_obj = midpoint_leader(group_obj);
		int res = *((unsigned char*)(*auls::Exedit_ObjectExtraData()) + group_obj->exdata_offset + 4);
		if (res == 0) res = LAYER_COUNT;
		return res;
	}

	// オブジェクトがグループ制御の直接の子となりうるかを判定する（シーンと時刻の一致は前提とする）
	bool group_can_contain(auls::EXEDIT_OBJECT* group_obj, auls::EXEDIT_OBJECT* target_obj) {
		group_obj = midpoint_leader(group_obj);

		const int control_begin_layer = group_obj->layer_set + 1;
		const int control_end_layer = control_begin_layer + get_group_control_range(group_obj);
		if (!(control_begin_layer <= target_obj->layer_set && target_obj->layer_set < control_end_layer)) return false;

		// 「同じグループのオブジェクトを対象にする」による除外条件
		if (group_obj->check_value[1] && group_obj->group_belong != target_obj->group_belong) return false;

		// 「上位グループ制御の影響を受ける」による除外条件
		if (is_group_object(target_obj) && !midpoint_leader(target_obj)->check_value[0]) return false;

		return true;
	}

	// CALL命令（e8）の呼び出し先を書き換え、書き換え前のアドレスを返す。
	DWORD rewrite_call_target(const DWORD call_operation_address, const void* target_function) {
		// 書き換え前のアドレスを絶対アドレスに変換して保持
		const DWORD oldFunction = *(DWORD*)(call_operation_address + 1) + (call_operation_address + 5);

		// 保護状態変更
		DWORD dwOldProtect;
		VirtualProtect((LPVOID)call_operation_address, 5, PAGE_READWRITE, &dwOldProtect);

		// 書き換え
		*(DWORD*)(call_operation_address + 1) = (DWORD)target_function - (call_operation_address + 5);

		// 保護状態復元
		VirtualProtect((LPVOID)call_operation_address, 5, dwOldProtect, &dwOldProtect);

		return oldFunction;
	}

}