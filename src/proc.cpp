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
	//  ���[�e�B���e�B
	//================================

	auls::EXEDIT_OBJECT* get_object(const int scene, const int layer, const int frame) {
		if (layer < 0 || layer >= LAYER_COUNT) return nullptr;

		const auto objs = auls::Exedit_SortedObjectTable();

		if (scene == *auls::Exedit_SceneDisplaying()) {
			// �A�N�e�B�u�V�[������T��

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
			// ��A�N�e�B�u�V�[������T��

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

	// �u�ҏW�S�̂̉��������ɂ���v�Ƀ`�F�b�N�������������g�`�\��
	bool inline is_wide_area_sound_wave_object(auls::EXEDIT_OBJECT* obj) {
		obj = midpoint_leader(obj);
		return obj->type == 1 && obj->filter_param[0].id == sound_wave_object_filter_id && obj->check_value[3];
	}

	// �O���[�v�̐��䃌�C���[�����擾����
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
	// �O���[�v����ɂ�郌�C���[�̖؍\��
	//================================

	// �m�[�h��1�̃O���[�v����܂��̓I�u�W�F�N�g�ɑΉ�����
	struct LAYER_GROUPING_FOREST_NODE {
		bool active = false;
		bool is_group = false;
		bool is_audio = false;
		auls::EXEDIT_OBJECT* object = nullptr;

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

	// �^�C�����C������؍\���𐶐�����
	LAYER_GROUPING_FOREST get_layer_grouping_forest(const int scene, const int frame, const int layer_limit = LAYER_COUNT) {
		LAYER_GROUPING_FOREST forest(LAYER_COUNT);
		std::stack<int> current_group_layers;

		for (int layer = 0;; ++layer) {
			// �O���[�v����̏I�[����
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

			// �I�[�f�B�I�A�C�e���͖؍\���Ɋ܂߂Ȃ�
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

	// �؍\���Ɋ�Â��ă��C���[���Z�N�V�����ɕ���
	std::vector<std::vector<int>> get_layer_sections_from_forest(const LAYER_GROUPING_FOREST& forest) {
		std::vector<LAYER_SECTION> sections;
		sections.push_back(LAYER_SECTION(0, LAYER_COUNT));

		// ���C���[���ɑ����i=�؂ɑ΂���DFS�j

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

				// �O���[�v����̒�v�f��
				// �W���̕`��@�\�ł͐e�O���[�v�̐��䂩��O��Ă��܂�����
				// �Z�N�V���������ɂ���đΏ�����

				for (unsigned int i = 0; i < children.size(); ++i) {
					
					const auto c_layer = children[i];
					parent[c_layer] = layer;

					if (i > 0 && forest[children[i - 1]].is_group) {
						// ==== �Z�N�V�������� ====

						// ���݂̃��C���[�ŏ㉺�ɕ�����
						sections.rbegin()->end = c_layer;
						LAYER_SECTION new_section(c_layer, LAYER_COUNT);

						// �c��̃O���[�v����͗L���ɂ���
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

		// ���X�g�`���ɕϊ�

		std::vector<std::vector<int>> res;

		int first_audio_section = -1;
		int first_wave_section = -1;

		for (const auto& section : sections) {
			std::vector<int> section_layers;
			section.for_each([&](const int layer) {
				if (!forest[layer].active) return;
				if (!forest[layer].is_audio) section_layers.push_back(layer);

				// �I�[�f�B�I�E�����g�`�\���̈ʒu���L�^���Ă���
				if (first_audio_section < 0 && forest[layer].is_audio) first_audio_section = res.size();
				if (first_wave_section < 0 && is_wide_area_sound_wave_object(forest[layer].object)) first_wave_section = res.size();
				});
			if (!section_layers.empty()) {
				res.push_back(section_layers);
			}
		}

		if (res.empty()) res.push_back(std::vector<int>());

		// �I�[�f�B�I���C���[�͂����ꂩ�̃Z�N�V�����ɂ܂Ƃ߂�
		// �i�����g�`�\���͓���Z�N�V�����̉��������E���Ȃ����߁A
		//�@�@����ɕ�������ƈꕔ�̉��������E���č����̌��ƂȂ�B�j
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
	//  �`�揈��
	//================================

	// DisplayRects�����֐�
	std::vector<auls::DISPLAY_RECT> merge_display_rects(const std::vector<std::vector<auls::DISPLAY_RECT>> section_display_rects) {
		// �y���Ӂz������256�𒴂���ƃo�O��
		const unsigned int MAX_RECT_COUNT = 256;

		std::vector<auls::DISPLAY_RECT> res;

		std::unordered_set<int> added_object_ids;

		for (const auto& section_display_rect : section_display_rects) {
			std::unordered_set<int> section_object_ids;

			for (const auto& rect : section_display_rect) {
				// �ȑO�̃Z�N�V�����Œǉ�����Ă���I�u�W�F�N�g�̂��̂̓p�X
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

	// �����`��֐��̈���
	struct EXEDIT_DRAWING_FUNC_ARGS {
		void* fp; // �g�b�v���x���`��łȂ����NULL
		FILTER_PROC_INFO* fpip;
		int layer_limit; // �`�悷�郌�C���[�� [0, layer_limit) �ɐ���
		int scene_change_time_offset; // �V�[���`�F���W�̌o�ߎ���
		int unknown; // ���0�H
		int scene; // ��g�b�v���x���`��̑ΏۃV�[��
		int object_id; // ��g�b�v���x���`��̌����ƂȂ����I�u�W�F�N�g�i�V�[��,�A�V�[���`�F���W���j�B�g�b�v���x���Ȃ��0
	};

	// �����`��֐�������ɏ���������
	// �iexedit->proc�̏��������ł̓V�[���`�F���W�ȂǂɑΉ��ł��Ȃ��j
	HOOKED(BOOL, , exedit_drawing_func, EXEDIT_DRAWING_FUNC_ARGS args) {
		if (g_fp == nullptr || !g_fp->exfunc->is_filter_active(g_fp)) {
			// �o�C�p�X
			return exedit_drawing_func_original(args);
		}

		static int recursion_level = 0;
		static std::unordered_map<int, std::vector<int>> layer_enabled_init;

		const int scene = args.fp != NULL ? *auls::Exedit_SceneDisplaying() : args.scene;
		const int frame = args.fpip->frame;

		// �ċA�̏�ʉߒ��œ����C���[�̕\����Ԃ��ύX����Ă���\�������邽�߁A
		// �Z�N�V���������v�Z�̂��߂ɕ������Ă���
		if (layer_enabled_init.contains(scene)) {
			for (int layer = 0; layer < LAYER_COUNT; ++layer) {
				set_layer_enabled(scene, layer, layer_enabled_init[scene][layer]);
			}
		}

		const auto sections = get_layer_sections(scene, frame, args.layer_limit);

		BOOL res = TRUE;

		if (sections.size() <= 1) {
			// �����̕K�v���Ȃ��ꍇ�͒ʏ�̏��������s
			++recursion_level;
			res = exedit_drawing_func_original(args);
			--recursion_level;
		}
		else {
			// ���C���[�̕\����Ԃ��L�^
			if (!layer_enabled_init.contains(scene)) {
				layer_enabled_init[scene].resize(LAYER_COUNT);
				for (int layer = 0; layer < LAYER_COUNT; ++layer) {
					layer_enabled_init[scene][layer] = get_layer_enabled(scene, layer);
				}
			}

			// �ꎞ�I�ɑS���C���[���\���ɂ���
			for (int layer = 0; layer < LAYER_COUNT; ++layer) {
				set_layer_enabled(scene, layer, false);
			}

			std::vector<std::vector<auls::DISPLAY_RECT>> section_display_rects;

			// �`��
			for (const auto& section : sections) {
				for (const auto& layer : section) set_layer_enabled(scene, layer, true);
				++recursion_level;
				res &= exedit_drawing_func_original(args);
				--recursion_level;
				for (const auto& layer : section) set_layer_enabled(scene, layer, false);

				if (recursion_level == 0) {
					// �ҏW�g���L�^
					std::vector<auls::DISPLAY_RECT> section_display_rect;
					for (int i = 0; i < *auls::Exedit_DisplayRectsCount(); ++i) {
						section_display_rect.push_back(auls::Exedit_DisplayRects()[i]);
					}
					section_display_rects.push_back(section_display_rect);
				}
			}

			// ���C���E�B���h�E��̕ҏW�g��S�Z�N�V�������܂Ƃ߂�i�g�b�v���x�����̂݁j
			if (recursion_level == 0) {
				const auto merged_display_rects = merge_display_rects(section_display_rects);
				const auto rects_count = merged_display_rects.size();
				*auls::Exedit_DisplayRectsCount() = rects_count;
				for (unsigned int i = 0; i < rects_count; ++i) {
					auls::Exedit_DisplayRects()[i] = merged_display_rects[i];
				}
			}
		}

		// �㏈��
		if (recursion_level == 0) {
			// ���C���[�̕\����Ԃ𕜌�
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
	//  ����������
	//================================

	void init(FILTER* fp, FILTER* exedit) {

		g_fp = fp;

		const DWORD exedit_base = (DWORD)exedit->dll_hinst;

		// �����`��֐����Ă�ł���CALL���߂̏��݈ꗗ
		const std::vector<DWORD> drawing_func_calls({
			exedit_base + 0x49C10,
			exedit_base + 0x2AF52,
			exedit_base + 0x4CDC4,
			exedit_base + 0x4CEFF
			});

		// �����`��֐��̃A�h���X
		// exedit_drawing_func_original = (t_exedit_drawing_func)(exedit_base + 0x48830);
		exedit_drawing_func_original = (t_exedit_drawing_func)(drawing_func_calls[0] + 5 + *(DWORD*)(drawing_func_calls[0] + 1)); // ���v���O�C���Ƃ̋������

		// �����`��֐����_�~�[�̂��̂ɍ����ւ���
		for (const auto& pt : drawing_func_calls) {
			// �ی��ԕύX
			DWORD dwOldProtect;
			VirtualProtect((LPVOID)pt, 5, PAGE_READWRITE, &dwOldProtect);

			// ��������
			*(DWORD*)(pt + 1) = (DWORD)exedit_drawing_func_hooked - (pt + 5);

			// �ی��ԕ���
			VirtualProtect((LPVOID)pt, 5, dwOldProtect, &dwOldProtect);
		}


		// �����g�`�\���̃t�B���^ID���擾
		const auto filters = auls::Exedit_StaticFilterTable();
		for (int i = 0; filters[i] != NULL; ++i) {
			if (_tcscmp(filters[i]->name, TEXT("�����g�`�\��")) == 0) {
				sound_wave_object_filter_id = i;
				break;
			}
		}

	}


	//================================
	//  �f�o�b�O�p�i�����[�X�������j
	//================================

#ifdef _DEBUG

	void debug() {
		cdbg << std::flush;
	}

#endif

}
