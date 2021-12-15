#include "aviutl_plugin_sdk/filter.h"
#include "auls/memref.h"
#include <vector>
#include <stack>
#include <iostream>

constexpr int LAYER_COUNT = 100;

//================================
//  ���[�e�B���e�B
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

// �O���[�v�̐��䃌�C���[�����擾����
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
// �O���[�v����ɂ�郌�C���[�̖؍\��
//================================

// �m�[�h��1�̃O���[�v����܂��̓I�u�W�F�N�g�ɑΉ�����
struct LAYER_GROUPING_FOREST_NODE {
	bool active = false;
	bool is_group = false;

	// �ȉ�, is_group == true �̏ꍇ�̂ݗL��
	int layer_end = -1; // �O���[�v����̐���͈͂𔼊J��� [layer, layer_end) �ŕ\��
	std::vector<int> children;
};

// �؍\���ivector�̈����̓��C���[�̃C���f�b�N�X�j
using LAYER_GROUPING_FOREST = std::vector<LAYER_GROUPING_FOREST_NODE>;

// �؍\���̃e�L�X�g�\���i�f�o�b�O�p�j
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

// �^�C�����C������؍\���𐶐�����
LAYER_GROUPING_FOREST get_layer_grouping_forest(const int& frame) {
	const auto scene = *auls::Exedit_SceneDisplaying();

	LAYER_GROUPING_FOREST forest(100);
	std::stack<int> current_group_layers;

	for (int layer = 0;; ++layer) {
		// �O���[�v����̏I�[����
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
// ���C���[�̃Z�N�V��������
//================================

struct LAYER_SECTION {
	int begin, end; // ���C���[ [begin, end) ��`�悷��
	std::vector<int> additional_layers; // ��O�I�ɕ`�悷�郌�C���[

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

// ���C���[�Z�N�V�����̃e�L�X�g�\���i�f�o�b�O�p�j
std::ostream& operator<<(std::ostream& s, const LAYER_SECTIONS& layer_sections) {
	for (const auto& section : layer_sections.sections) {
		s << "[" << section.begin << ", " << section.end << ")";
		for (const auto& al : section.additional_layers) s << ", " << al;
		s << "\n";
	}
	return s;
}

// �؍\���Ɋ�Â��ă��C���[���Z�N�V�����ɕ���
LAYER_SECTIONS get_layer_sections_from_forest(const LAYER_GROUPING_FOREST& forest) {
	LAYER_SECTIONS res;
	res.layer_enabled_default.resize(LAYER_COUNT);
	for (int i = 0; i < LAYER_COUNT; ++i)res.layer_enabled_default[i] = forest[i].active;
	res.sections.push_back(LAYER_SECTION(0, LAYER_COUNT));

	// ���C���[���ɑ����i=�؂ɑ΂���DFS�j

	std::vector<bool> layer_seen(LAYER_COUNT);
	for (int root = 0; root < LAYER_COUNT; ++root) {
		if (layer_seen[root] || !forest[root].active)continue;
		std::vector<int> layer_stack;
		layer_stack.push_back(root);
		while (!layer_stack.empty()) {
			const auto layer = *layer_stack.rbegin();
			layer_seen[layer] = true;
			const auto& children = forest[layer].children;

			// �O���[�v����̒�v�f��
			// �W���̕`��@�\�ł͐e�O���[�v�̐��䂩��O��Ă��܂�����
			// �Z�N�V���������ɂ���đΏ�����

			for (unsigned int i = 0; i < children.size(); ++i) {
				if (i > 0 && forest[children[i - 1]].is_group) {
					// ==== �Z�N�V�������� ====

					const auto c_layer = children[i];

					// ���݂̃��C���[�ŏ㉺�ɕ�����
					res.sections.rbegin()->end = c_layer;
					LAYER_SECTION new_section(c_layer, LAYER_COUNT);

					// �c��̃O���[�v����͗L���ɂ���
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
//  �`�揈��
//================================

typedef BOOL(*t_proc)(void* fp, FILTER_PROC_INFO* fpip);

// ���C���[�𕡐��̃Z�N�V�����ɕ����ĕ`��
bool draw_multi_section(LAYER_SECTIONS layer_sections, void* fp, FILTER_PROC_INFO* fpip, t_proc exedit_proc_original) {
	const auto scene = *auls::Exedit_SceneDisplaying();

	if (layer_sections.sections.size() <= 1) {
		return exedit_proc_original(fp, fpip);
	}

	// ���C���[�̕\����Ԃ��L�^��, �ꎞ�I�ɑS���C���[���\���ɂ���
	std::vector<bool> layer_enabled_init(LAYER_COUNT);
	for (int i = 0; i < LAYER_COUNT; ++i) {
		layer_enabled_init[i] = get_layer_enabled(scene, i);
		set_layer_enabled(scene, i, false);
	}

	const auto pt_display_rects = auls::Exedit_DisplayRects();
	const auto pt_display_rects_count = auls::Exedit_DisplayRectsCount();
	std::vector<auls::DISPLAY_RECT> display_rects;

	bool res = true;

	// �`��
	for (const auto& section : layer_sections.sections) {
		section.for_each([&](const int layer) { set_layer_enabled(scene, layer, layer_sections.layer_enabled_default[layer]); });
		const auto section_res = exedit_proc_original(fp, fpip);
		section.for_each([&](const int layer) { set_layer_enabled(scene, layer, false); });
		if (!section_res) {
			res = false;
			break;
		}

		// DisplayRect���L�^
		const auto display_rects_count = *pt_display_rects_count;
		for (int i = 0; i < display_rects_count; ++i) {
			display_rects.push_back(pt_display_rects[i]);
		}
	}

	// ��Ԃ𕜌�
	for (int i = 0; i < LAYER_COUNT; ++i) set_layer_enabled(scene, i, layer_enabled_init[i]);

	// �S�Z�N�V������DisplayRect������
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
//  �f�o�b�O�p�i�����[�X�������j
//================================

#ifdef _DEBUG

#include "dbgstream/dbgstream.h"

void proc_debug() {
}

#endif