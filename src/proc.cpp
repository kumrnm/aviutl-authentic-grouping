#include "aviutl_plugin_sdk/filter.h"
#include "tchar.h"
#include "auls/memref.h"
#include "auls/yulib/extra.h"
#include <vector>
#include <stack>

#ifdef _DEBUG
#include "dbgstream/dbgstream.h"
#endif

namespace proc {

	constexpr int LAYER_COUNT = 100;
	FILTER* g_fp = nullptr;


	//================================
	//  ユーティリティ
	//================================

	inline auls::EXEDIT_OBJECT* get_object_by_id(const int id) {
		return *auls::Exedit_ObjectTable() + id;
	}

	inline auls::EXEDIT_OBJECT* midpoint_leader(auls::EXEDIT_OBJECT* obj) {
		return obj->index_midpt_leader < 0 ? obj : get_object_by_id(obj->index_midpt_leader);
	}

	inline bool is_group_object(auls::EXEDIT_OBJECT* obj) {
		return obj->type == 28;
	}

	// グループの制御レイヤー数を取得する
	inline int get_group_control_range(auls::EXEDIT_OBJECT* group_obj) {
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


	//================================
	//  拡張編集の内部処理への干渉
	//================================

	HOOKED(void, , FireFilterUpdateEvent, int _arg0, int _arg1, void* pt, char _arg3) {
		if (g_fp != nullptr && g_fp->exfunc->is_filter_active(g_fp)) {
			// スタックから内部描画関数のローカル変数にアクセス
			const auto layer_object = (auls::EXEDIT_OBJECT**)((DWORD)pt + 0x4d0);
			const auto layer_parent_object = (auls::EXEDIT_OBJECT**)((DWORD)pt + 0x1b0);

			// 親になる可能性があるグループ制御とその終端レイヤーを記録
			std::vector<std::pair<auls::EXEDIT_OBJECT*, int>> group_objs;

			for (int layer_index = 0; layer_index < LAYER_COUNT; ++layer_index) {
				const auto obj = layer_object[layer_index];
				if (obj == NULL) continue;

				// グループ制御の終端処理
				while (!group_objs.empty() && layer_index >= group_objs.rbegin()->second) {
					group_objs.pop_back();
				}

				// 近い順に親を探索
				layer_parent_object[layer_index] = NULL;
				for (auto group_objs_ptr = group_objs.rbegin(); group_objs_ptr != group_objs.rend(); ++group_objs_ptr) {
					auto group_obj = group_objs_ptr->first;
					if (group_can_contain(group_obj, obj)) {
						layer_parent_object[layer_index] = group_obj;
						break;
					}
				}

				if (is_group_object(obj)) {
					group_objs.emplace_back(obj, layer_index + 1 + get_group_control_range(obj));
				}
			}
		}

		FireFilterUpdateEvent_original(_arg0, _arg1, pt, _arg3);
	}


	//================================
	//  初期化処理
	//================================

	// CALL命令の呼び出し先を書き換え、書き換え前のアドレスを返す。
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

	void init(FILTER* fp, FILTER* exedit) {
		g_fp = fp;
		const DWORD exedit_base = (DWORD)exedit->dll_hinst;
		FireFilterUpdateEvent_original = (t_FireFilterUpdateEvent)rewrite_call_target(exedit_base + 0x48e99, FireFilterUpdateEvent_hooked);
	}


	//================================
	//  デバッグ用（リリース時無効）
	//================================

#ifdef _DEBUG

	void debug() {
		cdbg << std::flush;
	}

#endif

}
