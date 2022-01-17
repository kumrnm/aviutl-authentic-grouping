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
	//  拡張編集の内部処理への干渉
	//================================

	// タイムラインの背景の描画に介入し、真・グループ制御の制御範囲を正しく描画する
	void* render_timeline_background_injected(win32_function_injection::REGISTER& reg) {
		// 標準では、タイムラインのレイヤー上の矩形に対して上位レイヤーに存在する制御系オブジェクトを上から順に走査し、
		// 子グループを発見し次第親グループの情報を破棄する処理が実行される。
		// この描画処理を真・グループ制御仕様に変更する。

		// 矩形の塗りつぶしと枠線の有無を管理する変数
		// flags は3ビットからなり、下位ビットから順に時間制御、グループ制御、カメラ制御に対応する
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
			// 描画対象のレイヤーより上で終端するグループ制御については、存在を無視する
			if (group_end_layer < layer) {
				ignore = true;
			}
		}

		if (!ignore) {
			// 標準処理
			fill_flags &= group_end_mask;
			left_line_flags &= group_end_mask;
			right_line_flags &= group_end_mask;
			bottom_line_flags &= group_end_mask;
		}

		// 機械語コードと同様のレジスタ操作を再現し、整合性を保つ
		reg.EBP = bottom_line_flags;

		// 復帰するアドレスを返す
		return (void*)(exedit_base + 0x3856d);
	}


	//================================
	//  初期化処理
	//================================

	void init(FILTER* fp, FILTER* exedit) {
		g_fp = fp;
		g_exedit = exedit;
		exedit_base = (DWORD)exedit->dll_hinst;

		win32_function_injection::inject_function((void*)(exedit_base + 0x38555), render_timeline_background_injected);
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