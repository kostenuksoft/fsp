#include "fsp_editor.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fsp_account.h"
#include "fsp_commands.h"
#include "fsp_cursors.h"
#include "fsp_debuglog.h"
#include "fsp_explorer.h"
#include "fsp_gdi.h"
#include "fsp_glyphs.h"
#include "fsp_main.h"
#include "fsp_menu.h"
#include "fsp_paint.h"
#include "fsp_plugclass.h"
#include "fsp_tooltip.h"

const c8 Editor_CreditFormat[] = "Licensed to %s";

const UINT Editor_AnimStepMs = 30;
const UINT Editor_FitFirstMs = 50;
const UINT Editor_FitRepeatMs = 500;
const UINT Editor_TipDelayMs = 450;
const i32 Editor_FadeStep = 32;

const i32 Fit_BelowEditor = 200;
const i32 Fit_Spacing = 5;
const i32 Fit_MaxDelta = 200;

const f64 Drag_FinePixelsPerStep = 3.0;
const f64 Drag_DigitStepPerPixel = 0.1;
const f64 Drag_KnobTravelPixels = 220.0;
const f64 Drag_RoundHalf = 0.5;
const i32 Drag_SendPixelsPerStep = 6;
const i32 Wheel_StepsPerRange = 64;

const c8 Hint_Logo[] = "^^www.livesynth.com^";
const c8 Hint_Info[] = "Show or hide the build information";
const c8 Hint_InfoFrame[] = "Fruity Soundfont Player, " Plug_Arch " re-build";
const c8 Hint_Open[] = "Open SoundFont...";
const c8 Hint_Edit[] = "Edit SoundFont...";
const c8 Hint_SoundfontName[] = "Soundfont name";
const c8 Hint_PatchName[] = "Patch name";
const c8 Hint_ReverbSend[] = "Send reverb to effect track number";
const c8 Hint_ChorusSend[] = "Send chorus to effect track number";
const c8 Hint_ReverbSwitch[] = "Enable built-in reverb";
const c8 Hint_ChorusSwitch[] = "Enable built-in chorus";
const c8 Hint_Quality[] = "High quality rendering (slow, not realtime)";

static bool Editor_InBox(const TBox *Box, i32 X, i32 Y)
{
    return X >= Box->X && X < Box->X + Box->W && Y >= Box->Y && Y < Box->Y + Box->H;
}

static bool Editor_InControl(const TControl *Control, i32 X, i32 Y)
{
    TBox Grown = { Control->Box.X - Layout_HitMargin, Control->Box.Y - Layout_HitMargin,
                   Control->Box.W + 2 * Layout_HitMargin, Control->Box.H + 2 * Layout_HitMargin };

    return Editor_InBox(&Grown, X, Y);
}

bool Editor_IsControl(i32 Hit)
{
    return Hit >= 0 && Hit < Layout_ControlCount;
}

static bool Editor_IsSend(i32 Hit)
{
    return Hit == Hit_ReverbSend || Hit == Hit_ChorusSend;
}

static i32 Editor_SendOf(i32 Hit)
{
    return (Hit == Hit_ReverbSend || Hit == Hit_ReverbSwitch) ? Send_Reverb : Send_Chorus;
}

bool Editor_IsDragging(const TSoundfontPlug *Plug)
{
    return Plug->Editor.Drag != Hit_None && Plug->EditorHandle;
}

i32 Editor_Height(const TSoundfontPlug *Plug)
{
    return Layout_BaseH + (Plug->Editor.InfoOpen ? Layout_InfoH : 0);
}

i32 Editor_HitTest(const TSoundfontPlug *Plug, i32 X, i32 Y)
{
    for (i32 Control = 0; Control < Layout_ControlCount; ++Control) {
        if (Editor_InControl(&Layout_Controls[Control], X, Y)) {
            return Control;
        }
    }
    if (Editor_InBox(&Layout_OpenButton, X, Y)) {
        return Hit_Open;
    }
    if (Editor_InBox(&Layout_EditButton, X, Y)) {
        return Hit_Edit;
    }
    if (Editor_InBox(&Layout_SoundfontName, X, Y)) {
        return Hit_SoundfontName;
    }
    if (Editor_InBox(&Layout_PatchName, X, Y)) {
        return Hit_PatchName;
    }
    if (Editor_InBox(&Layout_ReverbSend, X, Y)) {
        return Hit_ReverbSend;
    }
    if (Editor_InBox(&Layout_ChorusSend, X, Y)) {
        return Hit_ChorusSend;
    }
    if (Editor_InBox(&Layout_ReverbSwitch, X, Y)) {
        return Hit_ReverbSwitch;
    }
    if (Editor_InBox(&Layout_ChorusSwitch, X, Y)) {
        return Hit_ChorusSwitch;
    }
    if (Editor_InBox(&Layout_InfoButton, X, Y)) {
        return Hit_Info;
    }
    if (Plug->Editor.InfoOpen && Editor_InBox(&Layout_InfoFrame, X, Y)) {
        return Hit_InfoFrame;
    }
    if (Editor_InBox(&Layout_QualitySwitch, X, Y)) {
        return Hit_Quality;
    }
    if (Editor_InBox(&Layout_Logo, X, Y)) {
        return Hit_Logo;
    }
    return Hit_None;
}

static const c8 *Editor_HintFor(i32 Hit)
{
    if (Editor_IsControl(Hit)) {
        return Param_Defs[Layout_Controls[Hit].Param].Name;
    }
    switch (Hit) {
    case Hit_Logo:
        return Hint_Logo;
    case Hit_Info:
        return Hint_Info;
    case Hit_InfoFrame:
        return Hint_InfoFrame;
    case Hit_Open:
        return Hint_Open;
    case Hit_Edit:
        return Hint_Edit;
    case Hit_SoundfontName:
        return Hint_SoundfontName;
    case Hit_PatchName:
        return Hint_PatchName;
    case Hit_ReverbSend:
        return Hint_ReverbSend;
    case Hit_ChorusSend:
        return Hint_ChorusSend;
    case Hit_ReverbSwitch:
        return Hint_ReverbSwitch;
    case Hit_ChorusSwitch:
        return Hint_ChorusSwitch;
    case Hit_Quality:
        return Hint_Quality;
    }
    return NULL;
}

void Editor_Init(TSoundfontPlug *Plug)
{
    PEditor Editor = &Plug->Editor;
    c8 Name[Account_NameSize];

    memset(Editor, 0, sizeof(*Editor));
    Editor->Pressed = Hit_None;
    Editor->Hover = Hit_None;
    Editor->Drag = Hit_None;
    Editor->InfoOpen = true;
    Editor->TipControl = Hit_None;
    Account_GetName(Name, sizeof(Name));
    snprintf(Editor->Credit, sizeof(Editor->Credit), Editor_CreditFormat, Name);
}

void Editor_Free(TSoundfontPlug *Plug)
{
    Explorer_OwnerDestroyed(Plug);
    Editor_Hide(Plug);
    if (Plug->Editor.Artwork) {
        DeleteObject(Plug->Editor.Artwork);
        Plug->Editor.Artwork = NULL;
    }
    Canvas_Destroy(&Plug->Editor.Canvas);
}

HBITMAP Editor_Artwork(TSoundfontPlug *Plug)
{
    c8 Path[Text_PathSize];

    if (!Plug->Editor.Artwork) {
        snprintf(Path, sizeof(Path), "%s%s", Main_Directory(), Artwork_FileName);
        Plug->Editor.Artwork = (HBITMAP)LoadImageA(NULL, Path, IMAGE_BITMAP, 0, 0,
                                                   LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    }
    return Plug->Editor.Artwork;
}

void Editor_Redraw(TSoundfontPlug *Plug)
{
    if (Plug->EditorHandle) {
        InvalidateRect(Plug->EditorHandle, NULL, FALSE);
    }
}

void Editor_RedrawNow(TSoundfontPlug *Plug)
{
    if (Plug->EditorHandle) {
        UpdateWindow(Plug->EditorHandle);
    }
}

void Editor_HideTip(TSoundfontPlug *Plug)
{
    Tooltip_Hide(Plug);
    if (Plug->EditorHandle) {
        KillTimer(Plug->EditorHandle, Editor_TipTimer);
    }
}

static void Editor_SetSize(TSoundfontPlug *Plug)
{
    SetWindowPos(Plug->EditorHandle, NULL, 0, 0, Layout_PanelW, Editor_Height(Plug),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static i32 Fit_Delta(HWND Parent, HWND EditorWindow, const RECT *EditorArea)
{
    for (HWND Sibling = GetWindow(Parent, GW_CHILD); Sibling; Sibling = GetWindow(Sibling, GW_HWNDNEXT)) {
        RECT Area;

        if (Sibling == EditorWindow || !IsWindowVisible(Sibling)) {
            continue;
        }
        GetWindowRect(Sibling, &Area);
        if (Area.top >= EditorArea->top + Fit_BelowEditor) {
            return EditorArea->bottom + Fit_Spacing - Area.top;
        }
    }
    return 0;
}

static void Fit_MoveSiblings(HWND Parent, HWND EditorWindow, const RECT *EditorArea, i32 Delta)
{
    for (HWND Sibling = GetWindow(Parent, GW_CHILD); Sibling; Sibling = GetWindow(Sibling, GW_HWNDNEXT)) {
        RECT Area;
        POINT Spot;

        if (Sibling == EditorWindow) {
            continue;
        }
        GetWindowRect(Sibling, &Area);
        if (Area.top < EditorArea->top + Fit_BelowEditor) {
            continue;
        }
        Spot.x = Area.left;
        Spot.y = Area.top + Delta;
        ScreenToClient(Parent, &Spot);
        SetWindowPos(Sibling, NULL, Spot.x, Spot.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

static void Fit_GrowWindow(HWND Window, i32 Delta)
{
    RECT Area;

    if (!Window) {
        return;
    }
    GetWindowRect(Window, &Area);
    SetWindowPos(Window, NULL, 0, 0, Area.right - Area.left, Area.bottom - Area.top + Delta,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void Editor_FitHost(TSoundfontPlug *Plug)
{
    HWND Parent = Plug->EditorHandle ? GetParent(Plug->EditorHandle) : NULL;
    RECT Area;
    i32 Delta;

    if (!Parent) {
        return;
    }
    GetWindowRect(Plug->EditorHandle, &Area);
    if (Area.bottom - Area.top != Editor_Height(Plug) || Area.right - Area.left != Layout_PanelW) {
        DebugLog_Write("FitHost: editor is %ldx%ld, restoring", Area.right - Area.left, Area.bottom - Area.top);
        Editor_SetSize(Plug);
        GetWindowRect(Plug->EditorHandle, &Area);
    }
    Delta = Fit_Delta(Parent, Plug->EditorHandle, &Area);
    if (Delta == 0 || Delta > Fit_MaxDelta || Delta < -Fit_MaxDelta) {
        return;
    }
    DebugLog_Write("FitHost: growing host layout by %d", Delta);
    Fit_MoveSiblings(Parent, Plug->EditorHandle, &Area, Delta);
    Fit_GrowWindow(Parent, Delta);
    Fit_GrowWindow(GetParent(Parent), Delta);
}

void Editor_ToggleInfo(TSoundfontPlug *Plug)
{
    Plug->Editor.InfoOpen = !Plug->Editor.InfoOpen;
    if (Plug->EditorHandle) {
        Editor_SetSize(Plug);
        Editor_FitHost(Plug);
        Plug->PlugHost->Dispatcher(Plug->HostTag, FHD_EditorResized, 0, 0);
    }
    Plug_MarkDirty(Plug);
    Editor_Redraw(Plug);
}

void Editor_Show(TSoundfontPlug *Plug, HWND Parent)
{
    if (Plug->EditorHandle) {
        SetParent(Plug->EditorHandle, Parent);
        return;
    }
    Plug->EditorHandle = CreateWindowExA(0, Editor_ClassName, "", WS_CHILD | WS_VISIBLE, 0, 0, Layout_PanelW,
                                         Editor_Height(Plug), Parent, NULL, Main_Instance(), Plug);
    DebugLog_Write("ShowEditor parent=%p editor=%p", (void *)Parent, (void *)Plug->EditorHandle);
    Plug->PlugHost->Dispatcher(Plug->HostTag, FHD_EditorResized, 0, 0);
    SetTimer(Plug->EditorHandle, Editor_FitTimer, Editor_FitFirstMs, NULL);
}

void Editor_Hide(TSoundfontPlug *Plug)
{
    Tooltip_Free(Plug);
    if (Plug->EditorHandle) {
        DestroyWindow(Plug->EditorHandle);
    }
    Plug->EditorHandle = 0;
}

static bool Editor_FadeToward(i32 *Value, i32 Target)
{
    i32 Before = *Value;

    if (*Value < Target) {
        *Value = *Value + Editor_FadeStep > Target ? Target : *Value + Editor_FadeStep;
    } else if (*Value > Target) {
        *Value = *Value - Editor_FadeStep < Target ? Target : *Value - Editor_FadeStep;
    }
    return *Value != Before;
}

static bool Editor_Animate(TSoundfontPlug *Plug)
{
    PEditor Editor = &Plug->Editor;
    bool Moving = false;

    Moving |= Editor_FadeToward(&Editor->LogoFade, Editor->Hover == Hit_Logo ? Canvas_Opaque : 0);
    Moving |= Editor_FadeToward(&Editor->LedFade[Led_Reverb], Plug->Sends[Send_Reverb].Enabled ? Canvas_Opaque : 0);
    Moving |= Editor_FadeToward(&Editor->LedFade[Led_Chorus], Plug->Sends[Send_Chorus].Enabled ? Canvas_Opaque : 0);
    Moving |= Editor_FadeToward(&Editor->LedFade[Led_Quality], Plug->HighQuality ? Canvas_Opaque : 0);
    return Moving;
}

static void Editor_StartAnimation(HWND Window)
{
    SetTimer(Window, Editor_AnimTimer, Editor_AnimStepMs, NULL);
}

static void Editor_OnPaint(TSoundfontPlug *Plug, HWND Window)
{
    PAINTSTRUCT Paint;
    HDC DC = BeginPaint(Window, &Paint);
    TMemoryDC Buffer = { CreateCompatibleDC(DC) };
    TSelection Selected = { Buffer.Handle, NULL };

    if (!Plug->Editor.Canvas.Bitmap) {
        Canvas_Create(&Plug->Editor.Canvas, DC, Layout_PanelW, Layout_MaxH);
    }
    Selected.Previous = SelectObject(Buffer.Handle, Plug->Editor.Canvas.Bitmap);
    Paint_Editor(Plug, Buffer.Handle);
    BitBlt(DC, 0, 0, Layout_PanelW, Editor_Height(Plug), Buffer.Handle, 0, 0, SRCCOPY);
    EndPaint(Window, &Paint);
}

static void Editor_OnTimer(TSoundfontPlug *Plug, HWND Window, WPARAM Timer)
{
    PEditor Editor = &Plug->Editor;

    if (Timer == Editor_TipTimer) {
        KillTimer(Window, Editor_TipTimer);
        if (Editor_IsControl(Editor->Hover) && Editor->Drag == Hit_None) {
            Tooltip_Show(Plug, Editor->Hover, false);
        }
        return;
    }
    if (Timer == Editor_FitTimer) {
        Editor_FitHost(Plug);
        SetTimer(Window, Editor_FitTimer, Editor_FitRepeatMs, NULL);
        return;
    }
    if (!Editor_Animate(Plug)) {
        KillTimer(Window, Editor_AnimTimer);
    }
    Editor_Redraw(Plug);
}

static void Editor_OnPosChanging(TSoundfontPlug *Plug, WINDOWPOS *Position)
{
    if (Position->flags & SWP_NOSIZE) {
        return;
    }
    if (Position->cx == Layout_PanelW && Position->cy == Editor_Height(Plug)) {
        return;
    }
    DebugLog_Write("host resize request %dx%d flags=%x", Position->cx, Position->cy, Position->flags);
    Position->cx = Layout_PanelW;
    Position->cy = Editor_Height(Plug);
}

static void Editor_OnMouseLeave(TSoundfontPlug *Plug, HWND Window)
{
    Editor_HideTip(Plug);
    Plug->Editor.Hover = Hit_None;
    Editor_StartAnimation(Window);
    Editor_Redraw(Plug);
}

static void Editor_BeginControlDrag(TSoundfontPlug *Plug, HWND Window, i32 Control, i32 Y)
{
    PEditor Editor = &Plug->Editor;
    const TControl *Def = &Layout_Controls[Control];

    Editor->Drag = Control;
    Editor->DragY = Y;
    Editor->DragStart = Plug->ParamValues[Def->Param];
    Editor->DragValue = Editor->DragStart;
    GetCursorPos(&Editor->DragOrigin);
    SetCapture(Window);
    if (Def->Kind == Control_Knob) {
        ShowCursor(FALSE);
    } else if (Def->Kind == Control_Slider) {
        SetCursor(Cursors_Grab());
    }
    Plug_Hint(Plug, Param_Defs[Def->Param].Name);
    KillTimer(Window, Editor_TipTimer);
    Tooltip_Show(Plug, Control, true);
    Editor_Redraw(Plug);
}

static void Editor_OnLeftDown(TSoundfontPlug *Plug, HWND Window, i32 X, i32 Y)
{
    PEditor Editor = &Plug->Editor;
    i32 Hit = Editor_HitTest(Plug, X, Y);

    if (Editor_IsControl(Hit)) {
        Editor_BeginControlDrag(Plug, Window, Hit, Y);
    } else if (Editor_IsSend(Hit)) {
        Editor->Drag = Hit;
        Editor->DragY = Y;
        Editor->DragStart = Plug->Sends[Editor_SendOf(Hit)].Track + 1;
        SetCapture(Window);
    } else if (Hit == Hit_Open || Hit == Hit_Edit) {
        Editor->Pressed = Hit;
        SetCapture(Window);
        Editor_Redraw(Plug);
    } else if (Hit == Hit_SoundfontName) {
        Commands_SoundfontMenu(Plug);
    } else if (Hit == Hit_Info) {
        Editor_ToggleInfo(Plug);
    } else if (Hit == Hit_PatchName) {
        Explorer_Open(Plug);
    } else if (Hit == Hit_ReverbSwitch || Hit == Hit_ChorusSwitch) {
        Plug_ToggleSend(Plug, Editor_SendOf(Hit));
        Editor_StartAnimation(Window);
    } else if (Hit == Hit_Quality) {
        Plug_ToggleQuality(Plug);
        Editor_StartAnimation(Window);
    }
}

static void Editor_FollowSlider(TSoundfontPlug *Plug, HWND Window, const TControl *Def)
{
    PEditor Editor = &Plug->Editor;
    f64 Position = (Editor->DragValue - Param_Defs[Def->Param].Min) / Param_Range(Def->Param);
    i32 Travel = Def->Box.H - Glyph_SliderH;
    POINT Spot = { Def->Box.X + Def->Box.W / 2,
                   Def->Box.Y + (i32)((1.0 - Position) * Travel + Drag_RoundHalf) + Glyph_SliderH / 2 };

    ClientToScreen(Window, &Spot);
    SetCursorPos(Editor->DragOrigin.x, Spot.y);
    Editor->DragOrigin.y = Spot.y;
    SetCursor(Cursors_Grab());
}

static void Editor_DragControl(TSoundfontPlug *Plug, HWND Window, WPARAM Keys)
{
    PEditor Editor = &Plug->Editor;
    const TControl *Def = &Layout_Controls[Editor->Drag];
    const TParamDef *Param = &Param_Defs[Def->Param];
    bool Precise = (Keys & MK_SHIFT) || (Keys & MK_CONTROL);
    bool FreeSlider = Def->Kind == Control_Slider && !Precise;
    POINT Now;
    i32 Moved;

    GetCursorPos(&Now);
    Moved = Editor->DragOrigin.y - Now.y;
    if (Moved == 0) {
        return;
    }
    if (Precise) {
        Editor->DragValue += Moved / Drag_FinePixelsPerStep;
    } else if (Def->Kind == Control_Slider) {
        Editor->DragValue += Moved * Param_Range(Def->Param) / (f64)(Def->Box.H - Glyph_SliderH);
    } else if (Def->Kind == Control_Digits) {
        Editor->DragValue += Moved * Drag_DigitStepPerPixel;
    } else {
        Editor->DragValue += Moved * (Param_Range(Def->Param) / Drag_KnobTravelPixels);
    }
    if (!FreeSlider) {
        SetCursorPos(Editor->DragOrigin.x, Editor->DragOrigin.y);
    }
    if (Editor->DragValue < Param->Min) {
        Editor->DragValue = Param->Min;
    }
    if (Editor->DragValue > Param->Max) {
        Editor->DragValue = Param->Max;
    }
    Plug_SetParamFromEditor(Plug, Def->Param, (i32)floor(Editor->DragValue + Drag_RoundHalf));
    Tooltip_Show(Plug, Editor->Drag, true);
    if (FreeSlider) {
        Editor_FollowSlider(Plug, Window, Def);
    }
    Editor_Redraw(Plug);
    UpdateWindow(Window);
}

static void Editor_HoverMove(TSoundfontPlug *Plug, HWND Window, i32 X, i32 Y)
{
    PEditor Editor = &Plug->Editor;
    i32 Hit = Editor_HitTest(Plug, X, Y);
    const c8 *Hint = Editor_HintFor(Hit);

    if (Hit != Editor->Hover) {
        TRACKMOUSEEVENT Track;

        Track.cbSize = sizeof(Track);
        Track.dwFlags = TME_LEAVE;
        Track.hwndTrack = Window;
        Track.dwHoverTime = 0;
        TrackMouseEvent(&Track);
        Editor->Hover = Hit;
        Editor_HideTip(Plug);
        if (Editor_IsControl(Hit)) {
            SetTimer(Window, Editor_TipTimer, Editor_TipDelayMs, NULL);
        }
        Editor_StartAnimation(Window);
        Editor_Redraw(Plug);
    }
    if (Hint) {
        Plug_Hint(Plug, Hint);
    }
}

static void Editor_OnMouseMove(TSoundfontPlug *Plug, HWND Window, WPARAM Keys, i32 X, i32 Y)
{
    PEditor Editor = &Plug->Editor;

    if (Editor_IsControl(Editor->Drag)) {
        Editor_DragControl(Plug, Window, Keys);
    } else if (Editor_IsSend(Editor->Drag)) {
        i32 Steps = (Editor->DragY - Y) / Drag_SendPixelsPerStep;

        Plug_SetSendTrack(Plug, Editor_SendOf(Editor->Drag), Editor->DragStart + Steps);
    } else {
        Editor_HoverMove(Plug, Window, X, Y);
    }
}

static void Editor_ReleaseButton(TSoundfontPlug *Plug, HWND Window, i32 X, i32 Y)
{
    i32 Pressed = Plug->Editor.Pressed;

    Plug->Editor.Pressed = Hit_None;
    ReleaseCapture();
    Editor_Redraw(Plug);
    UpdateWindow(Window);
    if (Editor_HitTest(Plug, X, Y) != Pressed) {
        return;
    }
    if (Pressed == Hit_Open) {
        Commands_OpenDialog(Plug);
    } else {
        Commands_EditSoundfont(Plug);
    }
}

static void Editor_EndDrag(TSoundfontPlug *Plug)
{
    PEditor Editor = &Plug->Editor;

    if (Editor_IsControl(Editor->Drag)) {
        i32 Kind = Layout_Controls[Editor->Drag].Kind;

        if (Kind != Control_Slider) {
            SetCursorPos(Editor->DragOrigin.x, Editor->DragOrigin.y);
        }
        if (Kind == Control_Knob) {
            ShowCursor(TRUE);
        } else if (Kind == Control_Slider) {
            SetCursor(Cursors_Palm());
        }
    }
    Editor->Drag = Hit_None;
    ReleaseCapture();
    Editor_HideTip(Plug);
    Editor_Redraw(Plug);
}

static void Editor_OnLeftUp(TSoundfontPlug *Plug, HWND Window, i32 X, i32 Y)
{
    if (Plug->Editor.Pressed != Hit_None) {
        Editor_ReleaseButton(Plug, Window, X, Y);
    } else if (Plug->Editor.Drag != Hit_None) {
        Editor_EndDrag(Plug);
    }
}

static void Editor_OnSetCursor(TSoundfontPlug *Plug)
{
    PEditor Editor = &Plug->Editor;
    i32 Hover = Editor->Hover;
    LPCSTR Shape = IDC_ARROW;

    if (Editor_IsControl(Hover) && Layout_Controls[Hover].Kind == Control_Slider && Cursors_Palm()) {
        SetCursor(Editor->Drag == Hover ? Cursors_Grab() : Cursors_Palm());
        return;
    }
    if (Editor_IsControl(Hover)) {
        if (Layout_Controls[Hover].Kind == Control_Digits) {
            Shape = IDC_SIZENS;
        }
    } else if (Editor_IsSend(Hover)) {
        Shape = IDC_SIZENS;
    } else if (Hover != Hit_None && Hover != Hit_Logo) {
        Shape = IDC_HAND;
    }
    SetCursor(LoadCursorA(NULL, Shape));
}

static void Editor_OnWheel(TSoundfontPlug *Plug, HWND Window, WPARAM WParam, LPARAM LParam)
{
    POINT Spot = { (i16)LOWORD(LParam), (i16)HIWORD(LParam) };
    i32 Direction = (i16)HIWORD(WParam) > 0 ? 1 : -1;
    i32 Hit;

    ScreenToClient(Window, &Spot);
    Hit = Editor_HitTest(Plug, Spot.x, Spot.y);
    if (Editor_IsControl(Hit)) {
        const TControl *Def = &Layout_Controls[Hit];
        i32 Step = (Param_Defs[Def->Param].Max - Param_Defs[Def->Param].Min) / Wheel_StepsPerRange;

        if (Def->Kind == Control_Digits || Step < 1) {
            Step = 1;
        }
        Plug_SetParamFromEditor(Plug, Def->Param, Plug->ParamValues[Def->Param] + Direction * Step);
    } else if (Editor_IsSend(Hit)) {
        i32 Which = Editor_SendOf(Hit);

        Plug_SetSendTrack(Plug, Which, Plug->Sends[Which].Track + 1 + Direction);
    }
}

static void Editor_OnRightUp(TSoundfontPlug *Plug, HWND Window, i32 X, i32 Y)
{
    i32 Hit = Editor_HitTest(Plug, X, Y);
    POINT Spot = { X, Y };

    ClientToScreen(Window, &Spot);
    if (Editor_IsControl(Hit)) {
        Commands_ParamMenu(Plug, Layout_Controls[Hit].Param, Spot);
    } else if (Editor_IsSend(Hit)) {
        Plug_SetSendTrack(Plug, Editor_SendOf(Hit), 0);
    } else if (Hit == Hit_PatchName) {
        Commands_PatchMenu(Plug, Spot);
    } else if (Hit == Hit_SoundfontName) {
        Commands_SoundfontMenu(Plug);
    }
}

static void Editor_OnDoubleClick(TSoundfontPlug *Plug, i32 X, i32 Y)
{
    i32 Hit = Editor_HitTest(Plug, X, Y);

    if (Editor_IsControl(Hit)) {
        Commands_TypeValue(Plug, Layout_Controls[Hit].Param);
    }
}

static bool Editor_OnMouse(TSoundfontPlug *Plug, HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    i32 X = (i16)LOWORD(LParam);
    i32 Y = (i16)HIWORD(LParam);

    switch (Message) {
    case WM_MOUSELEAVE:
        Editor_OnMouseLeave(Plug, Window);
        return true;
    case WM_LBUTTONDOWN:
        Editor_OnLeftDown(Plug, Window, X, Y);
        return true;
    case WM_MOUSEMOVE:
        Editor_OnMouseMove(Plug, Window, WParam, X, Y);
        return true;
    case WM_LBUTTONUP:
        Editor_OnLeftUp(Plug, Window, X, Y);
        return true;
    case WM_MOUSEWHEEL:
        Editor_OnWheel(Plug, Window, WParam, LParam);
        return true;
    case WM_RBUTTONUP:
        Editor_OnRightUp(Plug, Window, X, Y);
        return true;
    case WM_LBUTTONDBLCLK:
        Editor_OnDoubleClick(Plug, X, Y);
        return true;
    }
    return false;
}

static LRESULT Editor_OnMessage(TSoundfontPlug *Plug, HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    if (Editor_OnMouse(Plug, Window, Message, WParam, LParam)) {
        return 0;
    }
    switch (Message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Editor_OnPaint(Plug, Window);
        return 0;
    case WM_TIMER:
        Editor_OnTimer(Plug, Window, WParam);
        return 0;
    case WM_WINDOWPOSCHANGING:
        Editor_OnPosChanging(Plug, (WINDOWPOS *)LParam);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(LParam) == HTCLIENT) {
            Editor_OnSetCursor(Plug);
            return TRUE;
        }
        break;
    case WM_MEASUREITEM:
        if (Menu_Measure((MEASUREITEMSTRUCT *)LParam)) {
            return TRUE;
        }
        break;
    case WM_DRAWITEM:
        if (Menu_Draw((DRAWITEMSTRUCT *)LParam)) {
            return TRUE;
        }
        break;
    }
    return DefWindowProcA(Window, Message, WParam, LParam);
}

static LRESULT CALLBACK Editor_WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    TSoundfontPlug *Plug;

    if (Message == WM_NCCREATE) {
        SetWindowLongPtrA(Window, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTA *)LParam)->lpCreateParams);
    }
    Plug = (TSoundfontPlug *)GetWindowLongPtrA(Window, GWLP_USERDATA);
    if (Message == WM_NCDESTROY) {
        SetWindowLongPtrA(Window, GWLP_USERDATA, 0);
    }
    if (!Plug) {
        return DefWindowProcA(Window, Message, WParam, LParam);
    }
    return Editor_OnMessage(Plug, Window, Message, WParam, LParam);
}

void Editor_Startup(HINSTANCE Instance)
{
    WNDCLASSA Class;

    memset(&Class, 0, sizeof(Class));
    Class.style = CS_DBLCLKS;
    Class.lpfnWndProc = Editor_WindowProc;
    Class.hInstance = Instance;
    Class.hCursor = LoadCursor(NULL, IDC_ARROW);
    Class.lpszClassName = Editor_ClassName;
    RegisterClassA(&Class);
}

void Editor_Shutdown(HINSTANCE Instance)
{
    UnregisterClassA(Editor_ClassName, Instance);
}
