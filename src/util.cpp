#include "windows.h"
#include "auls/memref.h"
#include "util.h"


namespace util {

	FILTER* g_fp = nullptr;

	void init(FILTER* fp) {
		g_fp = fp;
		auls::Memref_Init(fp);
	}

	bool is_filter_active() {
		return g_fp != nullptr && g_fp->exfunc->is_filter_active(g_fp);
	}

	auls::EXEDIT_OBJECT* get_object_by_id(const int id) {
		return auls::Exedit_ObjectBufferInfo()->data + id;
	}

	auls::EXEDIT_OBJECT* midpoint_leader(auls::EXEDIT_OBJECT* obj) {
		return obj->index_midpt_leader < 0 ? obj : get_object_by_id(obj->index_midpt_leader);
	}

	bool is_group_object(auls::EXEDIT_OBJECT* obj) {
		return obj->type == 28;
	}

	// �O���[�v�̐��䃌�C���[�����擾����
	int get_group_control_range(auls::EXEDIT_OBJECT* group_obj) {
		group_obj = midpoint_leader(group_obj);
		int res = *((unsigned char*)auls::Exedit_ObjectBufferInfo()->exdata + group_obj->exdata_offset + 4);
		if (res == 0) res = LAYER_COUNT;
		return res;
	}

	// �I�u�W�F�N�g���O���[�v����̒��ڂ̎q�ƂȂ肤�邩�𔻒肷��i�V�[���Ǝ����̈�v�͑O��Ƃ���j
	bool group_can_contain(auls::EXEDIT_OBJECT* group_obj, auls::EXEDIT_OBJECT* target_obj) {
		group_obj = midpoint_leader(group_obj);

		const int control_begin_layer = group_obj->layer_set + 1;
		const int control_end_layer = control_begin_layer + get_group_control_range(group_obj);
		if (!(control_begin_layer <= target_obj->layer_set && target_obj->layer_set < control_end_layer)) return false;

		// �u�����O���[�v�̃I�u�W�F�N�g��Ώۂɂ���v�ɂ�鏜�O����
		if (group_obj->check_value[1] && group_obj->group_belong != target_obj->group_belong) return false;

		// �u��ʃO���[�v����̉e�����󂯂�v�ɂ�鏜�O����
		if (is_group_object(target_obj) && !midpoint_leader(target_obj)->check_value[0]) return false;

		return true;
	}

	// CALL���߁ie8�j�̌Ăяo��������������A���������O�̃A�h���X��Ԃ��B
	DWORD rewrite_call_target(const DWORD call_operation_address, const void* target_function) {
		const DWORD oldFunction = *(DWORD*)(call_operation_address + 1) + (call_operation_address + 5);
		DWORD oldProtect;
		VirtualProtect((LPVOID)call_operation_address, 5, PAGE_READWRITE, &oldProtect);
		*(DWORD*)(call_operation_address + 1) = (DWORD)target_function - (call_operation_address + 5);
		VirtualProtect((LPVOID)call_operation_address, 5, oldProtect, &oldProtect);
		return oldFunction;
	}

}