#pragma once

#include <stdint.h>

#include "aulslib/aviutl.h"
#include "aulslib/exedit.h"


namespace auls {

void Memref_Init(FILTER* fp);

#define def(type, name)\
	type Exedit_##name(void)

def(auls::EXEDIT_FILTER**, StaticFilterTable);
def(int*, SortedObjectTable_LayerIndexEnd);
def(LPSTR, AliasNameBuffer);
def(int*, ObjectCount);
def(DISPLAY_RECT*, DisplayRects);
def(int*, ObjDlg_CommandTarget);
def(int*, SortedObjectTable_LayerIndexBegin);
def(BYTE*, LastActiveLayer);
def(BYTE*, ObjDlg_FilterStatus);
def(int*, ActiveSceneObjectCount);
def(auls::EXEDIT_OBJECT**, SortedObjectTable);
def(int*, ObjDlg_ObjectIndex);
def(auls::SCENE_SETTING*, SceneSetting);
def(int*, DisplayRectsCount);
def(auls::EXEDIT_FILTER**, LoadedFilterTable);
def(auls::LAYER_SETTING*, LayerSetting);
def(int*, SceneDisplaying);
def(wchar_t*, TextBuffer);
def(int*, TraScript_ProcessingTrackBarIndex);
def(int*, TraScript_ProcessingObjectIndex);
def(auls::EXEDIT_FILTER*, ScriptProcessingFilter);
// def(lua_State*,       LuaState);
def(EXEDIT_OBJECT**, ObjectTable);
def(void**, ObjectExtraData);
def(auls::OBJECT_BUFFER_INFO*, ObjectBufferInfo);
def(auls::CAMERA_ZBUFFER**, CameraZBuffer);
def(auls::UNDO_INFO*, UndoInfo);

#undef def

}