#include "aviutl_plugin_sdk/filter.h"
#include "win32_function_injection/win32_function_injection.h"

#ifdef _DEBUG
#include "dbgstream/dbgstream.h"
#endif


namespace gui {

	FILTER* g_fp = nullptr;
	FILTER* g_exedit = nullptr;
	DWORD exedit_base;

	bool rerender_timeline() {
		if (g_exedit == nullptr || g_exedit->hwnd == nullptr) return false;
		typedef void (*t_exedit_rendering_func)(HWND hwnd);
		((t_exedit_rendering_func)(exedit_base + 0x387f0))(g_exedit->hwnd);
		return true;
	}


	//================================
	//  �g���ҏW�̓��������ւ̊���
	//================================

	// �^�C�����C���̔w�i�̕`��ɉ�����A�^�E�O���[�v����̐���͈͂𐳂����`�悷��
	void* render_timeline_background_injected(win32_function_injection::REGISTER& reg) {
		// �W���ł́A�^�C�����C���̃��C���[��̋�`�ɑ΂��ď�ʃ��C���[�ɑ��݂��鐧��n�I�u�W�F�N�g���ォ�珇�ɑ������A
		// �q�O���[�v�𔭌�������e�O���[�v�̏���j�����鏈�������s�����B
		// ���̕`�揈����^�E�O���[�v����d�l�ɕύX����B

		// ��`�̓h��Ԃ��Ƙg���̗L�����Ǘ�����ϐ�
		// flags ��3�r�b�g����Ȃ�A���ʃr�b�g���珇�Ɏ��Ԑ���A�O���[�v����A�J��������ɑΉ�����
		unsigned int& fill_flags = reg.ESI;
		unsigned int& left_line_flags = *(unsigned int*)(reg.ESP + 0x10);
		unsigned int& right_line_flags = *(unsigned int*)(reg.ESP + 0x4c);
		unsigned int& bottom_line_flags = *(unsigned int*)(reg.ESP + 0x38);

		const int layer = *(int*)(reg.ESP + 0x3c);
		const void* group_obj_exdata_4_ptr = *(void**)(reg.ESP + 0x1c);
		const int group_end_layer = *(int*)((unsigned int)group_obj_exdata_4_ptr + 0xc);
		const unsigned int group_type_bit = reg.EDX;
		const unsigned int group_end_mask = 0xffff ^ group_type_bit;

		bool ignore = false;

		if (g_fp != nullptr && g_fp->exfunc->is_filter_active(g_fp) && (group_type_bit & 0x2)) {
			// �`��Ώۂ̃��C���[����ŏI�[����O���[�v����ɂ��ẮA���݂𖳎�����
			if (group_end_layer < layer) {
				ignore = true;
			}
		}

		if (!ignore) {
			// �W������
			fill_flags &= group_end_mask;
			left_line_flags &= group_end_mask;
			right_line_flags &= group_end_mask;
			bottom_line_flags &= group_end_mask;
		}

		// �@�B��R�[�h�Ɠ��l�̃��W�X�^������Č����A��������ۂ�
		reg.EBP = bottom_line_flags;

		// ���A����A�h���X��Ԃ�
		return (void*)(exedit_base + 0x3856d);
	}


	//================================
	//  ����������
	//================================

	void init(FILTER* fp, FILTER* exedit) {
		g_fp = fp;
		g_exedit = exedit;
		exedit_base = (DWORD)exedit->dll_hinst;

		win32_function_injection::inject_function((void*)(exedit_base + 0x38555), render_timeline_background_injected);
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