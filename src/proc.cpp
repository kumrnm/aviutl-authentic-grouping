#include "aviutl_plugin_sdk/filter.h"
#include "auls/memref.h"
#include <vector>
#include <stack>
#include <iostream>

constexpr int LAYER_COUNT = 100;

//================================
//  ユーティリティ
//================================

auls::EXEDIT_OBJECT* get_object(const int layer, const int frame) {
	if (layer < 0 || layer >= LAYER_COUNT) return nullptr;

	const auto objs = auls::Exedit_SortedObjectTable();

	int low = auls::Exedit_SortedObjectTable_LayerIndexBegin()[layer];
	int high = auls::Exedit_SortedObjectTable_LayerIndexEnd()[layer] + 1;

	while (high - low > 0) {
		const auto mid = (high + low) / 2;
		if (objs[mid]->frame_begin <= frame && frame <= objs[mid]->frame_end) {
			return objs[mid];
		}
		else if (frame < objs[mid]->frame_begin) {
			if (high == mid)return nullptr;
			high = mid;
		}
		else {
			if (low == mid)return nullptr;
			low = mid;
		}
	}

	return nullptr;
}

bool inline is_group_object(const auls::EXEDIT_OBJECT* obj) {
	return obj->type == 28;
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
LAYER_GROUPING_FOREST get_layer_grouping_forest(const int& frame) {
	const auto scene = *auls::Exedit_SceneDisplaying();

	LAYER_GROUPING_FOREST forest(100);
	std::stack<int> current_group_layers;

	for (int layer = 0;; ++layer) {
		// グループ制御の終端処理
		while (!current_group_layers.empty() && layer >= forest[current_group_layers.top()].layer_end) {
			forest[current_group_layers.top()].layer_end = std::max(forest[current_group_layers.top()].layer_end, layer);
			current_group_layers.pop();
		}

		if (layer >= LAYER_COUNT)break;

		if (!get_layer_enabled(scene, layer))continue;
		const auto obj = get_object(layer, frame);
		if (obj == nullptr)continue;
		forest[layer].active = true;
		forest[layer].is_group = is_group_object(obj);

		if (!current_group_layers.empty()) forest[current_group_layers.top()].children.push_back(layer);

		if (forest[layer].is_group) {
			forest[layer].layer_end = layer + 1 + get_group_size(obj);
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

struct LAYER_SECTIONS {
	std::vector<bool> layer_enabled_default;
	std::vector<LAYER_SECTION> sections;
};

// レイヤーセクションのテキスト表示（デバッグ用）
std::ostream& operator<<(std::ostream& s, const LAYER_SECTIONS& layer_sections) {
	for (const auto& section : layer_sections.sections) {
		s << "[" << section.begin << ", " << section.end << ")";
		for (const auto& al : section.additional_layers) s << ", " << al;
		s << "\n";
	}
	return s;
}

// 木構造に基づいてレイヤーをセクションに分割
LAYER_SECTIONS get_layer_sections_from_forest(const LAYER_GROUPING_FOREST& forest) {
	LAYER_SECTIONS res;
	res.layer_enabled_default.resize(LAYER_COUNT);
	for (int i = 0; i < LAYER_COUNT; ++i)res.layer_enabled_default[i] = forest[i].active;
	res.sections.push_back(LAYER_SECTION(0, LAYER_COUNT));

	// レイヤー順に走査（=木に対してDFS）

	std::vector<bool> layer_seen(LAYER_COUNT);
	for (int root = 0; root < LAYER_COUNT; ++root) {
		if (layer_seen[root] || !forest[root].active)continue;
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
					res.sections.rbegin()->end = c_layer;
					LAYER_SECTION new_section(c_layer, LAYER_COUNT);

					// 祖先のグループ制御は有効にする
					for (const auto& ancestor : layer_stack) {
						new_section.additional_layers.push_back(ancestor);
					}

					res.sections.push_back(new_section);
				}
			}

			layer_stack.pop_back();
			for (auto p = children.rbegin(); p != children.rend(); ++p) layer_stack.push_back(*p);
		}
	}

	return res;
}


//================================
//  描画処理
//================================

typedef BOOL(*t_proc)(void* fp, FILTER_PROC_INFO* fpip);

// レイヤーを複数のセクションに分けて描画
bool draw_multi_section(LAYER_SECTIONS layer_sections, void* fp, FILTER_PROC_INFO* fpip, t_proc exedit_proc_original) {
	const auto scene = *auls::Exedit_SceneDisplaying();

	if (layer_sections.sections.size() <= 1) {
		return exedit_proc_original(fp, fpip);
	}

	// レイヤーの表示状態を記録し, 一時的に全レイヤーを非表示にする
	std::vector<bool> layer_enabled_init(LAYER_COUNT);
	for (int i = 0; i < LAYER_COUNT; ++i) {
		layer_enabled_init[i] = get_layer_enabled(scene, i);
		set_layer_enabled(scene, i, false);
	}

	const auto pt_display_rects = auls::Exedit_DisplayRects();
	const auto pt_display_rects_count = auls::Exedit_DisplayRectsCount();
	std::vector<auls::DISPLAY_RECT> display_rects;

	bool res = true;

	// 描画
	for (const auto& section : layer_sections.sections) {
		section.for_each([&](const int layer) { set_layer_enabled(scene, layer, layer_sections.layer_enabled_default[layer]); });
		const auto section_res = exedit_proc_original(fp, fpip);
		section.for_each([&](const int layer) { set_layer_enabled(scene, layer, false); });
		if (!section_res) {
			res = false;
			break;
		}

		// DisplayRectを記録
		const auto display_rects_count = *pt_display_rects_count;
		for (int i = 0; i < display_rects_count; ++i) {
			display_rects.push_back(pt_display_rects[i]);
		}
	}

	// 状態を復元
	for (int i = 0; i < LAYER_COUNT; ++i) set_layer_enabled(scene, i, layer_enabled_init[i]);

	// 全セクションのDisplayRectを合成
	*pt_display_rects_count = display_rects.size();
	for (unsigned int i = 0; i < display_rects.size(); ++i) {
		pt_display_rects[i] = display_rects[i];
	}

	return res;
}

BOOL proc(void* fp, FILTER_PROC_INFO* fpip, t_proc exedit_proc_original) {
	const auto forest = get_layer_grouping_forest(fpip->frame);
	const auto sections = get_layer_sections_from_forest(forest);
	const auto res = draw_multi_section(sections, fp, fpip, exedit_proc_original);
	return res;
}


//================================
//  デバッグ用（リリース時無効）
//================================

#ifdef _DEBUG

#include "dbgstream/dbgstream.h"

void proc_debug() {
}

#endif