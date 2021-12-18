#include "aviutl_plugin_sdk/filter.h"
#include "tchar.h"
#include "auls/memref.h"
#include "auls/yulib/extra.h"
#include <vector>
#include <stack>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#ifdef _DEBUG
#include "dbgstream/dbgstream.h"
#endif

namespace proc {

	constexpr int LAYER_COUNT = 100;

	FILTER* g_fp = nullptr;


	//================================
	//  ユーティリティ
	//================================

	auls::EXEDIT_OBJECT* get_object(const int scene, const int layer, const int frame) {
		if (layer < 0 || layer >= LAYER_COUNT) return nullptr;

		const auto objs = auls::Exedit_SortedObjectTable();

		if (scene == *auls::Exedit_SceneDisplaying()) {
			// アクティブシーンから探索

			int low = auls::Exedit_SortedObjectTable_LayerIndexBegin()[layer];
			int high = auls::Exedit_SortedObjectTable_LayerIndexEnd()[layer] + 1;

			while (high - low > 0) {
				const auto mid = (high + low) / 2;
				if (objs[mid]->frame_begin <= frame && frame <= objs[mid]->frame_end) {
					return objs[mid];
				}
				else if (frame < objs[mid]->frame_begin) {
					if (high == mid) break;
					high = mid;
				}
				else {
					if (low == mid) break;
					low = mid;
				}
			}

		}
		else {
			// 非アクティブシーンから探索

			int low = *auls::Exedit_ActiveSceneObjectCount();
			int high = *auls::Exedit_ObjectCount();

			while (high - low > 0) {
				const auto mid = (high + low) / 2;
				if (scene < objs[mid]->scene_set) {
					if (high == mid) return nullptr;
					high = mid;
				}
				else if (objs[mid]->scene_set < scene) {
					if (low == mid) return nullptr;
					low = mid;
				}
				else if (layer < objs[mid]->layer_set) {
					if (high == mid) return nullptr;
					high = mid;
				}
				else if (objs[mid]->layer_set < layer) {
					if (low == mid) return nullptr;
					low = mid;
				}
				else if (objs[mid]->frame_begin <= frame && frame <= objs[mid]->frame_end) {
					return objs[mid];
				}
				else if (frame < objs[mid]->frame_begin) {
					if (high == mid) return nullptr;
					high = mid;
				}
				else {
					if (low == mid) return nullptr;
					low = mid;
				}
			}
		}

		return nullptr;
	}

	auls::EXEDIT_OBJECT* get_object_by_id(const int id) {
		return *auls::Exedit_ObjectTable() + id;
	}

	auls::EXEDIT_OBJECT* midpoint_leader(auls::EXEDIT_OBJECT* obj) {
		return obj->index_midpt_leader < 0 ? obj : get_object_by_id(obj->index_midpt_leader);
	}

	bool inline is_group_object(auls::EXEDIT_OBJECT* obj) {
		obj = midpoint_leader(obj);
		return obj->type == 28;
	}

	bool inline is_audio_object(auls::EXEDIT_OBJECT* obj) {
		obj = midpoint_leader(obj);
		return obj->type == 3;
	}

	int sound_wave_object_filter_id = -1;

	// 「編集全体の音声を元にする」にチェックが入った音声波形表示
	bool inline is_wide_area_sound_wave_object(auls::EXEDIT_OBJECT* obj) {
		obj = midpoint_leader(obj);
		return obj->type == 1 && obj->filter_param[0].id == sound_wave_object_filter_id && obj->check_value[3];
	}

	// グループの制御レイヤー数を取得する
	int inline get_group_size(auls::EXEDIT_OBJECT* group_obj) {
		group_obj = midpoint_leader(group_obj);
		int res = *((unsigned char*)(*auls::Exedit_ObjectExtraData()) + group_obj->exdata_offset + 4);
		if (res == 0) res = LAYER_COUNT;
		return res;
	}

	bool inline get_layer_enabled(const int scene, const int layer) {
		const auto ls = auls::Exedit_LayerSetting() + scene * LAYER_COUNT + layer;
		return !(ls->flag & auls::LAYER_SETTING::FLAG_UNDISP);
	}

	void inline set_layer_enabled(const int scene, const int layer, const bool value) {
		const auto ls = auls::Exedit_LayerSetting() + scene * LAYER_COUNT + layer;
		if (value) {
			ls->flag &= ~(DWORD)auls::LAYER_SETTING::FLAG_UNDISP;
		}
		else {
			ls->flag |= auls::LAYER_SETTING::FLAG_UNDISP;
		}
	}


	//================================
	// グループ制御によるレイヤーの木構造
	//================================

	// ノードは1つのグループ制御またはオブジェクトに対応する
	struct LAYER_GROUPING_FOREST_NODE {
		bool active = false;
		bool is_group = false;
		bool is_audio = false;
		auls::EXEDIT_OBJECT* object = nullptr;

		// 以下, is_group == true の場合のみ有効
		int layer_end = -1; // グループ制御の制御範囲を半開区間 [layer, layer_end) で表す
		std::vector<int> children;
	};

	// 木構造（vectorの引数はレイヤーのインデックス）
	using LAYER_GROUPING_FOREST = std::vector<LAYER_GROUPING_FOREST_NODE>;

	// 木構造のテキスト表示（デバッグ用）
	std::ostream& operator<<(std::ostream& s, const LAYER_GROUPING_FOREST& forest) {
		std::vector<bool> seen(forest.size());
		std::vector<bool> vertical_line(forest.size());
		for (unsigned int i = 0; i < forest.size(); ++i) {
			if (seen[i] || !forest[i].active)continue;
			std::stack<std::tuple<int, int, bool> /* layer, depth, is_last */> st;
			st.emplace(i, 0, true);
			while (!st.empty()) {
				const auto [layer, depth, is_last] = st.top();
				st.pop();
				seen[layer] = true;
				const auto& node = forest[layer];
				for (auto p = node.children.rbegin(); p != node.children.rend(); ++p) {
					st.emplace(*p, depth + 1, p == node.children.rbegin());
				}

				for (int i = 0; i < depth - 1; ++i) s << (vertical_line[i + 1] ? " |\t" : "\t");
				if (depth > 0)s << (is_last ? " +" : " |") << "--";
				s << (node.is_group ? "[" : "(") << layer << (node.is_group ? "]" : ")") << "\n";
				vertical_line[depth] = !is_last;
			}
		}

		return s;
	}

	// タイムラインから木構造を生成する
	LAYER_GROUPING_FOREST get_layer_grouping_forest(const int scene, const int frame, const int layer_limit = LAYER_COUNT) {
		LAYER_GROUPING_FOREST forest(LAYER_COUNT);
		std::stack<int> current_group_layers;

		for (int layer = 0;; ++layer) {
			// グループ制御の終端処理
			while (!current_group_layers.empty() && layer >= forest[current_group_layers.top()].layer_end) {
				forest[current_group_layers.top()].layer_end = std::max(forest[current_group_layers.top()].layer_end, layer);
				current_group_layers.pop();
			}

			if (layer >= layer_limit) break;

			if (!get_layer_enabled(scene, layer)) continue;
			const auto obj = get_object(scene, layer, frame);
			if (obj == nullptr)continue;
			forest[layer].active = true;
			forest[layer].is_group = is_group_object(obj);
			forest[layer].is_audio = is_audio_object(obj);
			forest[layer].object = obj;

			// オーディオアイテムは木構造に含めない
			if (forest[layer].is_audio) continue;

			if (!current_group_layers.empty()) forest[current_group_layers.top()].children.push_back(layer);

			if (forest[layer].is_group) {
				forest[layer].layer_end = std::min(layer + 1 + get_group_size(obj), layer_limit);
				current_group_layers.push(layer);
			}
		}

		return forest;
	}


	//================================
	// レイヤーのセクション分割
	//================================

	struct LAYER_SECTION {
		int begin, end; // レイヤー [begin, end) を描画する
		std::vector<int> additional_layers; // 例外的に描画するレイヤー

		LAYER_SECTION(const int begin, const int end) : begin(begin), end(end) {}

		template<typename Callable>
		void for_each(Callable f) const {
			for (const auto& al : additional_layers) f(al);
			for (int i = begin; i < end; ++i) f(i);
		}
	};

	// 木構造に基づいてレイヤーをセクションに分割
	std::vector<std::vector<int>> get_layer_sections_from_forest(const LAYER_GROUPING_FOREST& forest) {
		std::vector<LAYER_SECTION> sections;
		sections.push_back(LAYER_SECTION(0, LAYER_COUNT));

		// レイヤー順に走査（=木に対してDFS）

		std::vector<bool> layer_seen(LAYER_COUNT);
		std::vector<int> parent(LAYER_COUNT, -1);
		for (int root = 0; root < LAYER_COUNT; ++root) {
			if (layer_seen[root] || !forest[root].active) continue;
			std::stack<int> layer_stack;
			layer_stack.push(root);
			while (!layer_stack.empty()) {
				const auto layer = layer_stack.top();
				layer_seen[layer] = true;
				const auto& children = forest[layer].children;

				// グループ制御の弟要素は
				// 標準の描画機能では親グループの制御から外れてしまうため
				// セクション分割によって対処する

				for (unsigned int i = 0; i < children.size(); ++i) {
					
					const auto c_layer = children[i];
					parent[c_layer] = layer;

					if (i > 0 && forest[children[i - 1]].is_group) {
						// ==== セクション分割 ====

						// 現在のレイヤーで上下に分ける
						sections.rbegin()->end = c_layer;
						LAYER_SECTION new_section(c_layer, LAYER_COUNT);

						// 祖先のグループ制御は有効にする
						auto pos = c_layer;
						while (parent[pos] >= 0) {
							pos = parent[pos];
							new_section.additional_layers.push_back(pos);
						}

						sections.push_back(new_section);
					}
				}

				layer_stack.pop();
				for (auto p = children.rbegin(); p != children.rend(); ++p) {
					layer_stack.push(*p);
				}
			}
		}

		// リスト形式に変換

		std::vector<std::vector<int>> res;

		int first_audio_section = -1;
		int first_wave_section = -1;

		for (const auto& section : sections) {
			std::vector<int> section_layers;
			section.for_each([&](const int layer) {
				if (!forest[layer].active) return;
				if (!forest[layer].is_audio) section_layers.push_back(layer);

				// オーディオ・音声波形表示の位置を記録しておく
				if (first_audio_section < 0 && forest[layer].is_audio) first_audio_section = res.size();
				if (first_wave_section < 0 && is_wide_area_sound_wave_object(forest[layer].object)) first_wave_section = res.size();
				});
			if (!section_layers.empty()) {
				res.push_back(section_layers);
			}
		}

		if (res.empty()) res.push_back(std::vector<int>());

		// オーディオレイヤーはいずれかのセクションにまとめる
		// （音声波形表示は同一セクションの音声しか拾えないため、
		//　　下手に分割すると一部の音だけが拾われて混乱の元となる。）
		int audio_section = first_wave_section >= 0 ? first_wave_section : first_audio_section >= 0 ? first_audio_section : 0;
		for (int layer = 0; layer < LAYER_COUNT; ++layer) {
			if (forest[layer].active && forest[layer].is_audio) {
				res[audio_section].push_back(layer);
			}
		}

		return res;
	}

	std::vector<std::vector<int>> get_layer_sections(const int scene, const int frame, const int layer_limit = LAYER_COUNT) {
		return get_layer_sections_from_forest(get_layer_grouping_forest(scene, frame, layer_limit));
	}


	//================================
	//  描画処理
	//================================

	// DisplayRects合成関数
	std::vector<auls::DISPLAY_RECT> merge_display_rects(const std::vector<std::vector<auls::DISPLAY_RECT>> section_display_rects) {
		// 【注意】総数が256を超えるとバグる
		const unsigned int MAX_RECT_COUNT = 256;

		std::vector<auls::DISPLAY_RECT> res;

		std::unordered_set<int> added_object_ids;

		for (const auto& section_display_rect : section_display_rects) {
			std::unordered_set<int> section_object_ids;

			for (const auto& rect : section_display_rect) {
				// 以前のセクションで追加されているオブジェクトのものはパス
				if (added_object_ids.contains(rect.object_id)) continue;

				section_object_ids.insert(rect.object_id);

				res.push_back(rect);
				if (res.size() >= MAX_RECT_COUNT) goto break_for;
			}

			for (const auto id : section_object_ids) added_object_ids.insert(id);
		}
	break_for:

		return res;
	}

	// 内部描画関数の引数
	struct EXEDIT_DRAWING_FUNC_ARGS {
		void* fp; // トップレベル描画でなければNULL
		FILTER_PROC_INFO* fpip;
		int layer_limit; // 描画するレイヤーを [0, layer_limit) に制限
		int scene_change_time_offset; // シーンチェンジの経過時間
		int unknown; // 常に0？
		int scene; // 非トップレベル描画の対象シーン
		int object_id; // 非トップレベル描画の原因となったオブジェクト（シーン,、シーンチェンジ等）。トップレベルならば0
	};

	// 内部描画関数をこれに書き換える
	// （exedit->procの書き換えではシーンチェンジなどに対応できない）
	HOOKED(BOOL, , exedit_drawing_func, EXEDIT_DRAWING_FUNC_ARGS args) {
		if (g_fp == nullptr || !g_fp->exfunc->is_filter_active(g_fp)) {
			// バイパス
			return exedit_drawing_func_original(args);
		}

		static int recursion_level = 0;
		static std::unordered_map<int, std::vector<int>> layer_enabled_init;

		const int scene = args.fp != NULL ? *auls::Exedit_SceneDisplaying() : args.scene;
		const int frame = args.fpip->frame;

		// 再帰の上位過程で同レイヤーの表示状態が変更されている可能性があるため、
		// セクション分割計算のために復元しておく
		if (layer_enabled_init.contains(scene)) {
			for (int layer = 0; layer < LAYER_COUNT; ++layer) {
				set_layer_enabled(scene, layer, layer_enabled_init[scene][layer]);
			}
		}

		const auto sections = get_layer_sections(scene, frame, args.layer_limit);

		BOOL res = TRUE;

		if (sections.size() <= 1) {
			// 分割の必要がない場合は通常の処理を実行
			++recursion_level;
			res = exedit_drawing_func_original(args);
			--recursion_level;
		}
		else {
			// レイヤーの表示状態を記録
			if (!layer_enabled_init.contains(scene)) {
				layer_enabled_init[scene].resize(LAYER_COUNT);
				for (int layer = 0; layer < LAYER_COUNT; ++layer) {
					layer_enabled_init[scene][layer] = get_layer_enabled(scene, layer);
				}
			}

			// 一時的に全レイヤーを非表示にする
			for (int layer = 0; layer < LAYER_COUNT; ++layer) {
				set_layer_enabled(scene, layer, false);
			}

			std::vector<std::vector<auls::DISPLAY_RECT>> section_display_rects;

			// 描画
			for (const auto& section : sections) {
				for (const auto& layer : section) set_layer_enabled(scene, layer, true);
				++recursion_level;
				res &= exedit_drawing_func_original(args);
				--recursion_level;
				for (const auto& layer : section) set_layer_enabled(scene, layer, false);

				if (recursion_level == 0) {
					// 編集枠を記録
					std::vector<auls::DISPLAY_RECT> section_display_rect;
					for (int i = 0; i < *auls::Exedit_DisplayRectsCount(); ++i) {
						section_display_rect.push_back(auls::Exedit_DisplayRects()[i]);
					}
					section_display_rects.push_back(section_display_rect);
				}
			}

			// メインウィンドウ上の編集枠を全セクション分まとめる（トップレベル時のみ）
			if (recursion_level == 0) {
				const auto merged_display_rects = merge_display_rects(section_display_rects);
				const auto rects_count = merged_display_rects.size();
				*auls::Exedit_DisplayRectsCount() = rects_count;
				for (unsigned int i = 0; i < rects_count; ++i) {
					auls::Exedit_DisplayRects()[i] = merged_display_rects[i];
				}
			}
		}

		// 後処理
		if (recursion_level == 0) {
			// レイヤーの表示状態を復元
			for (const auto& [scene, values] : layer_enabled_init) {
				for (int layer = 0; layer < LAYER_COUNT; ++layer) {
					set_layer_enabled(scene, layer, values[layer]);
				}
			}
			layer_enabled_init.clear();
		}

		return res;
	}


	//================================
	//  初期化処理
	//================================

	void init(FILTER* fp, FILTER* exedit) {

		g_fp = fp;

		const DWORD exedit_base = (DWORD)exedit->dll_hinst;

		// 内部描画関数を呼んでいるCALL命令の所在一覧
		const std::vector<DWORD> drawing_func_calls({
			exedit_base + 0x49C10,
			exedit_base + 0x2AF52,
			exedit_base + 0x4CDC4,
			exedit_base + 0x4CEFF
			});

		// 内部描画関数のアドレス
		// exedit_drawing_func_original = (t_exedit_drawing_func)(exedit_base + 0x48830);
		exedit_drawing_func_original = (t_exedit_drawing_func)(drawing_func_calls[0] + 5 + *(DWORD*)(drawing_func_calls[0] + 1)); // 他プラグインとの競合回避

		// 内部描画関数をダミーのものに差し替える
		for (const auto& pt : drawing_func_calls) {
			// 保護状態変更
			DWORD dwOldProtect;
			VirtualProtect((LPVOID)pt, 5, PAGE_READWRITE, &dwOldProtect);

			// 書き換え
			*(DWORD*)(pt + 1) = (DWORD)exedit_drawing_func_hooked - (pt + 5);

			// 保護状態復元
			VirtualProtect((LPVOID)pt, 5, dwOldProtect, &dwOldProtect);
		}


		// 音声波形表示のフィルタIDを取得
		const auto filters = auls::Exedit_StaticFilterTable();
		for (int i = 0; filters[i] != NULL; ++i) {
			if (_tcscmp(filters[i]->name, TEXT("音声波形表示")) == 0) {
				sound_wave_object_filter_id = i;
				break;
			}
		}

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
