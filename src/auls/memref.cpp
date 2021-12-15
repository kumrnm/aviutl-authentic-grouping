#include "aulslib/aviutl.h"
#include "aulslib/exedit.h"
#include "yulib/extra.h"
#include "memref.h"
#include <vector>
#include <unordered_map>

using namespace auls;



namespace auls {

// 拡張編集メモリオフセット
struct EXEDIT_MEMORY_OFFSET {
	static const DWORD StaticFilterTable   = 0x0A3E28;
	static const DWORD SortedObjectTable_LayerIndexEnd = 0x135AC8;
	static const DWORD AliasNameBuffer     = 0x135C70;
	static const DWORD ObjectCount         = 0x146250;
	static const DWORD DisplayRects        = 0x146258;
	static const DWORD ObjDlg_CommandTarget = 0x14965C;
	static const DWORD SortedObjectTable_LayerIndexBegin = 0x149670;
	static const DWORD LastActiveLayer     = 0x149808;
	static const DWORD ObjDlg_FilterStatus = 0x158D38;
	static const DWORD ActiveSceneObjectCount = 0x15918C;
	static const DWORD SortedObjectTable   = 0x168FA8;
	static const DWORD ObjDlg_ObjectIndex      = 0x177A10;
	static const DWORD SceneSetting        = 0x177A50;
	static const DWORD DisplayRectsCount   = 0x17921C;
	static const DWORD LoadedFilterTable   = 0x187C98;
	static const DWORD LayerSetting        = 0x188498;
	static const DWORD SceneDisplaying     = 0x1A5310;
	static const DWORD TextBuffer          = 0x1A6BD0;
	static const DWORD TraScript_ProcessingTrackBarIndex = 0x1B21F4;
	static const DWORD TraScript_ProcessingObjectIndex = 0x1B2B04;
	static const DWORD ScriptProcessingFilter = 0x1B2B10;
	static const DWORD LuaState            = 0x1BACA8;
	static const DWORD ObjectExtraData     = 0x1E0FA8;
	static const DWORD ObjectBufferInfo    = 0x1E0F9C;
	static const DWORD CameraZBuffer       = 0x1EC7AC;
	static const DWORD UndoInfo            = 0x244E08;
};

DWORD g_exedit_inst = NULL;

void Memref_Init(FILTER * fp)
{
	if(g_exedit_inst != NULL) return;
	FILTER *exedit = Exedit_GetFilter(fp);
	g_exedit_inst = (DWORD)exedit->dll_hinst;
}


#define def(type, name)\
	type Exedit_##name(void)\
	{\
		return (type)(EXEDIT_MEMORY_OFFSET::name + g_exedit_inst);\
	}

def(EXEDIT_FILTER**,  StaticFilterTable);
def(int*,             SortedObjectTable_LayerIndexEnd);
def(LPSTR,            AliasNameBuffer);
def(int*,             ObjectCount);
def(DISPLAY_RECT*,    DisplayRects);
def(int*,             ObjDlg_CommandTarget);
def(int*,             SortedObjectTable_LayerIndexBegin);
def(BYTE*,            LastActiveLayer);
def(BYTE*,            ObjDlg_FilterStatus);
def(int*,             ActiveSceneObjectCount);
def(EXEDIT_OBJECT**,  SortedObjectTable);
def(int*,             ObjDlg_ObjectIndex);
def(SCENE_SETTING*,   SceneSetting);
def(int*,             DisplayRectsCount);
def(EXEDIT_FILTER**,  LoadedFilterTable);
def(LAYER_SETTING*,   LayerSetting);
def(int*,             SceneDisplaying);
def(wchar_t*,         TextBuffer);
def(int*,             TraScript_ProcessingTrackBarIndex);
def(int*,             TraScript_ProcessingObjectIndex);
def(EXEDIT_FILTER*,   ScriptProcessingFilter);
// def(lua_State*,       LuaState);
def(void**,           ObjectExtraData);
def(OBJECT_BUFFER_INFO*,  ObjectBufferInfo);
def(CAMERA_ZBUFFER**, CameraZBuffer);
def(UNDO_INFO*,       UndoInfo);
#undef def


}