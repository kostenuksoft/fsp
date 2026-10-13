#ifndef FSP_MAIN_H
#define FSP_MAIN_H

#include "fsp_def.h"

#include "sdk/fp_plugclass.h"

extern "C" {

TFruityPlug *_stdcall CreatePlugInstance(TFruityPlugHost *Host, TPluginTag Tag);
BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, LPVOID Reserved);

}

HINSTANCE Main_Instance();
const c8 *Main_Directory();

#endif
