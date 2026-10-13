#ifndef FSP_EDITOR_H
#define FSP_EDITOR_H

#include "fsp_canvas.h"
#include "fsp_def.h"
#include "fsp_layout.h"

const c8 Editor_ClassName[] = "FruitySoundfontPlayerX64";
const u32 Editor_TipTextSize = 200;
const u32 Editor_CreditSize = 160;

enum {
    Hit_None = -1,
    Hit_Open = Layout_ControlCount,
    Hit_Edit,
    Hit_SoundfontName,
    Hit_PatchName,
    Hit_ReverbSend,
    Hit_ChorusSend,
    Hit_ReverbSwitch,
    Hit_ChorusSwitch,
    Hit_Quality,
    Hit_Logo,
    Hit_Info,
    Hit_InfoFrame
};

enum {
    Led_Reverb,
    Led_Chorus,
    Led_Quality,
    Led_Count
};

enum {
    Editor_AnimTimer = 1,
    Editor_FitTimer = 2,
    Editor_TipTimer = 3
};

typedef struct {
    HBITMAP Artwork;
    TCanvas Canvas;
    i32 Pressed;
    i32 Hover;
    i32 LogoFade;
    i32 LedFade[Led_Count];
    bool InfoOpen;
    i32 Drag;
    i32 DragY;
    i32 DragStart;
    f64 DragValue;
    POINT DragOrigin;
    HWND Tip;
    c8 TipText[Editor_TipTextSize];
    i32 TipControl;
    i32 ExplorerSortMode;
    bool ExplorerSortDescending;
    c8 Credit[Editor_CreditSize];
} TEditor, *PEditor;

void Editor_Startup(HINSTANCE Instance);
void Editor_Shutdown(HINSTANCE Instance);
void Editor_Init(TSoundfontPlug *Plug);
void Editor_Free(TSoundfontPlug *Plug);
void Editor_Show(TSoundfontPlug *Plug, HWND Parent);
void Editor_Hide(TSoundfontPlug *Plug);
void Editor_Redraw(TSoundfontPlug *Plug);
void Editor_RedrawNow(TSoundfontPlug *Plug);
void Editor_ToggleInfo(TSoundfontPlug *Plug);
void Editor_HideTip(TSoundfontPlug *Plug);
i32 Editor_Height(const TSoundfontPlug *Plug);
i32 Editor_HitTest(const TSoundfontPlug *Plug, i32 X, i32 Y);
bool Editor_IsControl(i32 Hit);
bool Editor_IsDragging(const TSoundfontPlug *Plug);
HBITMAP Editor_Artwork(TSoundfontPlug *Plug);

#endif
