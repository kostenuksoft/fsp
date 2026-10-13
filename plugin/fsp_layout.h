#ifndef FSP_LAYOUT_H
#define FSP_LAYOUT_H

#include "fsp_artwork.h"
#include "fsp_def.h"
#include "fsp_params.h"

const i32 Layout_PanelW = Artwork_Width;
const i32 Layout_Gap = 16;
const i32 Layout_SplitY = 30;
const i32 Layout_BaseH = Artwork_Height + Layout_Gap;
const i32 Layout_InfoH = 72;
const i32 Layout_MaxH = Layout_BaseH + Layout_InfoH;

const i32 Layout_TextH = 13;
const i32 Layout_HitMargin = 2;

constexpr TBox Layout_OpenButton = { 4, 4, 31, 23 };
constexpr TBox Layout_EditButton = { 206, 4, 31, 23 };
constexpr TBox Layout_SoundfontName = { 41, 6, 162, 15 };
constexpr TBox Layout_SoundfontText = { 41, 7, 162, Layout_TextH };
constexpr TBox Layout_PatchName = { 71, 43 + Layout_Gap, 162, 15 };
constexpr TBox Layout_PatchText = { 71, 44 + Layout_Gap, 108, Layout_TextH };
constexpr TSpot Layout_SoundfontMenu = { 37, 24 };

constexpr TBox Layout_ReverbSend = { 58, 84 + Layout_Gap, 22, 20 };
constexpr TBox Layout_ChorusSend = { 177, 84 + Layout_Gap, 22, 20 };
constexpr TSpot Layout_ReverbSendDigits = { 62, 89 + Layout_Gap };
constexpr TSpot Layout_ChorusSendDigits = { 181, 89 + Layout_Gap };
constexpr TBox Layout_ReverbSwitch = { 5, 107 + Layout_Gap, 54, 11 };
constexpr TBox Layout_ChorusSwitch = { 124, 107 + Layout_Gap, 54, 11 };
constexpr TBox Layout_QualitySwitch = { 1, 227 + Layout_Gap, 79, 11 };
constexpr TSpot Layout_ReverbLed = { 9, 110 + Layout_Gap };
constexpr TSpot Layout_ChorusLed = { 128, 110 + Layout_Gap };
constexpr TSpot Layout_QualityLed = { 5, 230 + Layout_Gap };

constexpr TBox Layout_Logo = { 2, 206 + Layout_Gap, 112, 19 };
constexpr TBox Layout_InfoButton = { 118, 207 + Layout_Gap, 17, 17 };

const i32 Layout_WellH = Artwork_Well.H;
constexpr TBox Layout_Wells[] = {
    { 6, 42 + Layout_Gap, 27, Layout_WellH },
    { 36, 42 + Layout_Gap, 27, Layout_WellH },
    { 59, 85 + Layout_Gap, 20, Layout_WellH },
    { 178, 85 + Layout_Gap, 20, Layout_WellH },
};

constexpr TBox Layout_InfoFrame = { 16, Layout_BaseH + 12, 209, 48 };
const i32 Layout_InfoFrameLeftW = 100;
const i32 Layout_CreditX = Layout_InfoFrame.X + 24;
const i32 Layout_CreditW = Layout_InfoFrame.W - 30;
const i32 Layout_CreditFirstY = Layout_InfoFrame.Y + 5;
const i32 Layout_CreditStepY = Layout_TextH;
constexpr TSpot Layout_CreditMascot = { Layout_InfoFrame.X - 14, Layout_InfoFrame.Y - 15 };

constexpr TSpot Layout_Mascot = { 176, 1 + Layout_Gap };
const i32 Layout_MascotHandY = Layout_Gap + 54;

enum {
    Control_Digits,
    Control_Knob,
    Control_Slider
};

typedef struct {
    i32 Kind;
    i32 Param;
    TBox Box;
} TControl, *PControl;

constexpr TControl Layout_Controls[] = {
    { Control_Digits, Param_Patch, { 7, 43 + Layout_Gap, 25, 16 } },
    { Control_Digits, Param_Bank, { 37, 43 + Layout_Gap, 25, 16 } },
    { Control_Knob, Param_ReverbSend, { 91, 87 + Layout_Gap, 15, 15 } },
    { Control_Knob, Param_ChorusSend, { 210, 87 + Layout_Gap, 15, 15 } },
    { Control_Knob, Param_Mod, { 207, 201 + Layout_Gap, 17, 17 } },
    { Control_Slider, Param_Attack, { 14, 133 + Layout_Gap, 18, 57 } },
    { Control_Slider, Param_Decay, { 39, 133 + Layout_Gap, 18, 57 } },
    { Control_Slider, Param_Sustain, { 64, 133 + Layout_Gap, 18, 57 } },
    { Control_Slider, Param_Release, { 89, 133 + Layout_Gap, 18, 57 } },
    { Control_Knob, Param_LfoDelay, { 135, 145 + Layout_Gap, 17, 17 } },
    { Control_Knob, Param_LfoDepth, { 171, 145 + Layout_Gap, 17, 17 } },
    { Control_Knob, Param_LfoSpeed, { 207, 145 + Layout_Gap, 17, 17 } },
    { Control_Knob, Param_Cutoff, { 171, 201 + Layout_Gap, 17, 17 } },
};
const i32 Layout_ControlCount = (i32)ARRAY_COUNT(Layout_Controls);

constexpr TSpot Layout_ControlDigitsInset = { 2, 3 };
const i32 Layout_ControlDigitCount = 3;
const i32 Layout_SendDigitCount = 2;

const f64 Layout_KnobStartAngle = 3.92699081698724;
const f64 Layout_KnobSweepAngle = 4.71238898038469;
const f64 Layout_KnobRadiusPad = 1.0;
const f64 Layout_KnobNeedleInner = 0.1;
const f64 Layout_KnobNeedleHalfW = 0.8;

const i32 Layout_TipPadW = 14;
const i32 Layout_TipH = 18;
const i32 Layout_TipAboveControl = 4;
constexpr TSpot Layout_TipCursorOffset = { 12, 20 };

const i32 Layout_ExplorerW = Layout_PanelW;
const i32 Layout_ExplorerH = 268;
const i32 Layout_ExplorerBelowEditorY = 63 + Layout_Gap;
const i32 Layout_ExplorerAboveEditorY = 40 + Layout_Gap;
constexpr TBox Layout_ExplorerSearch = { 5, 4, 231, 20 };
constexpr TBox Layout_ExplorerQueryText = { 11, 7, 150, Layout_TextH };
constexpr TBox Layout_ExplorerCountText = { 150, 7, 80, Layout_TextH };
constexpr TSpot Layout_ExplorerTitles = { 4, 27 };
const i32 Layout_ExplorerTitlesTop = 26;
const i32 Layout_ExplorerRowH = 12;
constexpr TBox Layout_ExplorerList = { 5, 38, 231, Layout_ExplorerH - 38 - 7 };
const i32 Layout_ExplorerListPad = 3;
const i32 Layout_ExplorerRows = (Layout_ExplorerList.H - 2 * Layout_ExplorerListPad) / Layout_ExplorerRowH;
const i32 Layout_ExplorerPatchX = 9;
const i32 Layout_ExplorerBankX = 39;
const i32 Layout_ExplorerNameX = 70;
const i32 Layout_ExplorerNumberW = 26;
const i32 Layout_ExplorerTrackX = Layout_ExplorerList.X + Layout_ExplorerList.W - 9;
const i32 Layout_ExplorerRowLeft = Layout_ExplorerList.X + 2;
const i32 Layout_ExplorerRowRight = Layout_ExplorerTrackX - 3;
const i32 Layout_ExplorerNameW = Layout_ExplorerTrackX - Layout_ExplorerNameX - 6;
const i32 Layout_ExplorerHitRight = Layout_ExplorerTrackX - 2;
const i32 Layout_ExplorerTrackTop = Layout_ExplorerList.Y + 4;
const i32 Layout_ExplorerTrackLen = Layout_ExplorerList.H - 8;
const i32 Layout_ExplorerTrackLineX = Layout_ExplorerTrackX + 2;
const i32 Layout_ExplorerThumbW = 5;
const i32 Layout_ExplorerThumbMinH = 12;
const i32 Layout_ExplorerSortColumns = 3;
const i32 Layout_ExplorerTitleRight[Layout_ExplorerSortColumns] = { 35, 64, 110 };
constexpr TBox Layout_ExplorerSortMarks[Layout_ExplorerSortColumns] = {
    { 7, 36, 26, 1 },
    { 37, 36, 24, 1 },
    { 67, 36, 26, 1 },
};
constexpr TBox Layout_ExplorerSortArrow = { 100, 29, 6, 4 };

#endif
