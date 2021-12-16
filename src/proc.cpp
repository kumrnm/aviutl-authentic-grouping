#include "aviutl_plugin_sdk/filter.h"
#include "auls/memref.h"
#include "auls/yulib/extra.h"
#include <vector>
#include <stack>
#include <iostream>
#include <unordered_map>

constexpr int LAYER_COUNT = 100;

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

bool inline is_group_object(const auls::EXEDIT_OBJECT* obj) {
	return obj->type == 28;
}

bool inline is_audio_object(const auls::EXEDIT_OBJECT* obj) {
	return obj->type == 3;
}

// グループの制御レイヤー数を取得する
int inline get_group_size(const auls::EXEDIT_OBJECT* group_obj) {
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
			const auto& [layer, depth, is_last] = st.top();
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
	for (int root = 0; root < LAYER_COUNT; ++root) {
		if (layer_seen[root] || !forest[root].active) continue;
		std::vector<int> layer_stack;
		layer_stack.push_back(root);
		while (!layer_stack.empty()) {
			const auto layer = *layer_stack.rbegin();
			layer_seen[layer] = true;
			const auto& children = forest[layer].children;

			// グループ制御の弟要素は
			// 標準の描画機能では親グループの制御から外れてしまうため
			// セクション分割によって対処する

			for (unsigned int i = 0; i < children.size(); ++i) {
				if (i > 0 && forest[children[i - 1]].is_group) {
					// ==== セクション分割 ====

					const auto c_layer = children[i];

					// 現在のレイヤーで上下に分ける
					sections.rbegin()->end = c_layer;
					LAYER_SECTION new_section(c_layer, LAYER_COUNT);

					// 祖先のグループ制御は有効にする
					for (const auto& ancestor : layer_stack) {
						new_section.additional_layers.push_back(ancestor);
					}

					sections.push_back(new_section);
				}
			}

			layer_stack.pop_back();
			for (auto p = children.rbegin(); p != children.rend(); ++p) layer_stack.push_back(*p);
		}
	}

	// リスト形式に変換

	std::vector<std::vector<int>> res;

	for (const auto& section : sections) {
		std::vector<int> section_layers;
		int item_count = 0;
		section.for_each([&](const int layer) {
			if (forest[layer].active && !forest[layer].is_audio) {
				section_layers.push_back(layer);
				if (!forest[layer].is_group) ++item_count;
			}
			});
		res.push_back(section_layers);
	}

	if (res.empty()) res.push_back(std::vector<int>());

	// オーディオレイヤーはいずれかのセクションにまとめる
	// （音声波形表示は同一セクションの音声しか拾えないため、
	//　　下手に分割すると一部の音だけが拾われて混乱の元となる。）
	for (int layer = 0; layer < LAYER_COUNT; ++layer) {
		if (forest[layer].active && forest[layer].is_audio) {
			res[0].push_back(layer);
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

#include "dbgstream/dbgstream.h"

// 内部描画関数の引数
struct EXEDIT_PAINT_FUNC_ARGS {
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
HOOKED(BOOL, , exedit_drawing_func, EXEDIT_PAINT_FUNC_ARGS args) {
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

		std::vector<auls::DISPLAY_RECT> display_rects;

		// 描画
		for (const auto& section : sections) {
			for (const auto& layer : section) set_layer_enabled(scene, layer, true);
			++recursion_level;
			res &= exedit_drawing_func_original(args);
			--recursion_level;
			for (const auto& layer : section) set_layer_enabled(scene, layer, false);

			if (recursion_level == 0) {
				for (int i = 0; i < *auls::Exedit_DisplayRectsCount(); ++i) {
					display_rects.push_back(auls::Exedit_DisplayRects()[i]);
				}
			}
		}

		// メインウィンドウ上の編集枠を全セクション分まとめる（トップレベル時のみ）
		if (recursion_level == 0) {
			*auls::Exedit_DisplayRectsCount() = display_rects.size();
			for (unsigned int i = 0; i < display_rects.size(); ++i) {
				auls::Exedit_DisplayRects()[i] = display_rects[i];
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

void proc_init(FILTER* fp, FILTER* exedit) {

	const DWORD exedit_base = (DWORD)exedit->dll_hinst;

	// 内部描画関数を呼んでいるCALL命令の所在一覧
	const std::vector<DWORD> paint_func_calls({
		exedit_base + 0x49C10,
		exedit_base + 0x2AF52,
		exedit_base + 0x4CDC4,
		exedit_base + 0x4CEFF
		});

	// 内部描画関数のアドレス
	// exedit_drawing_func_original = (t_exedit_drawing_func)(exedit_base + 0x48830);
	exedit_drawing_func_original = (t_exedit_drawing_func)(paint_func_calls[0] + 5 + *(DWORD*)(paint_func_calls[0] + 1)); // 他プラグインとの競合回避

	// 内部描画関数をダミーのものに差し替える
	for (const auto& pt : paint_func_calls) {
		// 保護状態変更
		DWORD dwOldProtect;
		VirtualProtect((LPVOID)pt, 5, PAGE_READWRITE, &dwOldProtect);

		// 書き換え
		*(DWORD*)(pt + 1) = (DWORD)exedit_drawing_func_hooked - (pt + 5);

		// 保護状態復元
		VirtualProtect((LPVOID)pt, 5, dwOldProtect, &dwOldProtect);
	}
}


//================================
//  デバッグ用（リリース時無効）
//================================

#ifdef _DEBUG

#include "dbgstream/dbgstream.h"

void proc_debug() {
}

#endif