#include "aviutl_plugin_sdk/filter.h"
#include "tchar.h"
#include "util.h"
#include <vector>
#include <stack>

#ifdef _DEBUG
#include "dbgstream/dbgstream.h"
#endif

namespace proc {

	//================================
	//  拡張編集の内部処理への干渉
	//================================

	HOOKED(void, , FireFilterUpdateEvent, int _arg0, int _arg1, void* pt, char _arg3) {
		if (util::is_filter_active()) {
			// スタックから内部描画関数のローカル変数にアクセス
			const auto layer_object = (auls::EXEDIT_OBJECT**)((DWORD)pt + 0x4d0);
			const auto layer_parent_object = (auls::EXEDIT_OBJECT**)((DWORD)pt + 0x1b0);

			// 親になる可能性があるグループ制御とその終端レイヤーを記録
			std::vector<std::pair<auls::EXEDIT_OBJECT*, int>> group_objs;

			for (int layer_index = 0; layer_index < util::LAYER_COUNT; ++layer_index) {
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
					if (util::group_can_contain(group_obj, obj)) {
						layer_parent_object[layer_index] = group_obj;
						break;
					}
				}

				if (util::is_group_object(obj)) {
					group_objs.emplace_back(obj, layer_index + 1 + util::get_group_control_range(obj));
				}
			}
		}

		FireFilterUpdateEvent_original(_arg0, _arg1, pt, _arg3);
	}


	//================================
	//  初期化処理
	//================================

	void init(FILTER* exedit) {
		const DWORD exedit_base = (DWORD)exedit->dll_hinst;
		FireFilterUpdateEvent_original = (t_FireFilterUpdateEvent)util::rewrite_call_target(exedit_base + 0x48e99, FireFilterUpdateEvent_hooked);
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
