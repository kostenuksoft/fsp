#include "fsp_main.h"

#include <commctrl.h>
#include <string.h>

#include "../engine/flspro.h"

#include "fsp_cursors.h"
#include "fsp_editor.h"
#include "fsp_explorer.h"
#include "fsp_menu.h"
#include "fsp_paint.h"
#include "fsp_plugclass.h"
#include "fsp_registration.h"
#include "fsp_tooltip.h"
#include "fsp_voices.h"

const c8 Main_PathSeparator = '\\';

static HINSTANCE Main_InstanceHandle;
static c8 Main_DirectoryPath[MAX_PATH];

HINSTANCE Main_Instance()
{
    return Main_InstanceHandle;
}

const c8 *Main_Directory()
{
    return Main_DirectoryPath;
}

static void Main_FindDirectory(HINSTANCE Instance)
{
    c8 *Separator;

    GetModuleFileNameA(Instance, Main_DirectoryPath, sizeof(Main_DirectoryPath));
    Separator = strrchr(Main_DirectoryPath, Main_PathSeparator);
    if (Separator) {
        Separator[1] = 0;
    }
}

static void Main_Startup(HINSTANCE Instance)
{
    Main_InstanceHandle = Instance;
    Main_FindDirectory(Instance);
    Editor_Startup(Instance);
    Explorer_Startup(Instance);
    Tooltip_Startup(Instance);
    InitCommonControls();
    Cursors_Create(Instance);
    Voices_Startup();
}

static void Main_Shutdown(HINSTANCE Instance)
{
    fls_shutdown();
    Editor_Shutdown(Instance);
    Explorer_Shutdown(Instance);
    Tooltip_Shutdown(Instance);
    Menu_Shutdown();
    Paint_Shutdown();
}

extern "C" TFruityPlug *_stdcall CreatePlugInstance(TFruityPlugHost *Host, TPluginTag Tag)
{
    i32 Verdict = Registration_Check(Host, Tag, &Plug_Info.Flags);

    if (Verdict == Registration_Registered) {
        Plug_Info.Flags &= ~FPF_DemoVersion;
    } else if (Verdict == Registration_Demo) {
        Plug_Info.Flags |= FPF_DemoVersion;
    }
    return new TSoundfontPlug(Tag, Host);
}

extern "C" BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, LPVOID Reserved)
{
    (void)Reserved;
    if (Reason == DLL_PROCESS_ATTACH) {
        Main_Startup(Instance);
    } else if (Reason == DLL_PROCESS_DETACH) {
        Main_Shutdown(Instance);
    }
    return TRUE;
}
