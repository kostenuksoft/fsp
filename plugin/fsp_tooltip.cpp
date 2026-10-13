#include "fsp_tooltip.h"

#include <string.h>

#include "fsp_gdi.h"
#include "fsp_main.h"
#include "fsp_menu.h"
#include "fsp_plugclass.h"
#include "fsp_theme.h"

const c8 Tooltip_ClassName[] = "FruitySoundfontPlayerX64Tip";
const i32 Tooltip_NoControl = -1;

static void Tooltip_Draw(HDC DC, RECT *Area, const c8 *Text)
{
    TGdiObject Back = { CreateSolidBrush(Theme_MenuBack) };
    TGdiObject Edge = { CreateSolidBrush(Theme_MenuBorder) };
    TSelection Font = { DC, SelectObject(DC, Menu_Font()) };

    FillRect(DC, Area, (HBRUSH)Back.Handle);
    FrameRect(DC, Area, (HBRUSH)Edge.Handle);
    SetBkMode(DC, TRANSPARENT);
    SetTextColor(DC, Theme_MenuInk);
    DrawTextA(DC, Text, -1, Area, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
}

static void Tooltip_Paint(HWND Window)
{
    PAINTSTRUCT Paint;
    HDC DC = BeginPaint(Window, &Paint);
    c8 Text[Editor_TipTextSize];
    RECT Area;

    GetClientRect(Window, &Area);
    GetWindowTextA(Window, Text, sizeof(Text));
    Tooltip_Draw(DC, &Area, Text);
    EndPaint(Window, &Paint);
}

static LRESULT CALLBACK Tooltip_WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    switch (Message) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Tooltip_Paint(Window);
        return 0;
    }
    return DefWindowProcA(Window, Message, WParam, LParam);
}

void Tooltip_Startup(HINSTANCE Instance)
{
    WNDCLASSA Class;

    memset(&Class, 0, sizeof(Class));
    Class.lpfnWndProc = Tooltip_WindowProc;
    Class.hInstance = Instance;
    Class.hCursor = LoadCursor(NULL, IDC_ARROW);
    Class.lpszClassName = Tooltip_ClassName;
    RegisterClassA(&Class);
}

void Tooltip_Shutdown(HINSTANCE Instance)
{
    UnregisterClassA(Tooltip_ClassName, Instance);
}

static i32 Tooltip_TextWidth(const c8 *Text)
{
    TScreenDC Screen = { GetDC(NULL) };
    TSelection Font = { Screen.Handle, SelectObject(Screen.Handle, Menu_Font()) };
    RECT Bounds = { 0, 0, 0, 0 };

    DrawTextA(Screen.Handle, Text, -1, &Bounds, DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX);
    return Bounds.right;
}

static POINT Tooltip_Position(TSoundfontPlug *Plug, i32 Control, bool AtControl, i32 Width)
{
    const TBox *Box = &Layout_Controls[Control].Box;
    POINT Spot;

    if (AtControl || Editor_IsDragging(Plug)) {
        Spot.x = Box->X + Box->W / 2 - Width / 2;
        Spot.y = Box->Y - Layout_TipH - Layout_TipAboveControl;
        ClientToScreen(Plug->EditorHandle, &Spot);
    } else {
        GetCursorPos(&Spot);
        Spot.x += Layout_TipCursorOffset.X;
        Spot.y += Layout_TipCursorOffset.Y;
    }
    return Spot;
}

static bool Tooltip_EnsureWindow(TSoundfontPlug *Plug)
{
    PEditor Editor = &Plug->Editor;

    if (!Editor->Tip) {
        DWORD ExStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;

        Editor->Tip = CreateWindowExA(ExStyle, Tooltip_ClassName, "", WS_POPUP, 0, 0, Layout_TipH, Layout_TipH,
                                      Plug->EditorHandle, NULL, Main_Instance(), NULL);
    }
    return Editor->Tip != NULL;
}

void Tooltip_Show(TSoundfontPlug *Plug, i32 Control, bool AtControl)
{
    PEditor Editor = &Plug->Editor;
    c8 Text[Editor_TipTextSize];
    i32 Width;
    POINT Spot;
    bool Moved;

    if (!Editor_IsControl(Control) || !Plug->EditorHandle) {
        return;
    }
    Plug_FormatValue(Plug, Layout_Controls[Control].Param, Text, sizeof(Text));
    Width = Tooltip_TextWidth(Text) + Layout_TipPadW;
    Spot = Tooltip_Position(Plug, Control, AtControl, Width);
    if (!Tooltip_EnsureWindow(Plug)) {
        return;
    }
    Moved = Editor->TipControl != Control || !IsWindowVisible(Editor->Tip) || Editor_IsDragging(Plug);
    if (!Text_Same(Text, Editor->TipText) || Moved) {
        UINT Flags = SWP_NOACTIVATE | SWP_SHOWWINDOW;

        Text_Copy(Editor->TipText, sizeof(Editor->TipText), Text);
        SetWindowTextA(Editor->Tip, Text);
        if (!Moved) {
            Flags |= SWP_NOMOVE;
        }
        SetWindowPos(Editor->Tip, HWND_TOPMOST, Spot.x, Spot.y, Width, Layout_TipH, Flags);
        InvalidateRect(Editor->Tip, NULL, FALSE);
        UpdateWindow(Editor->Tip);
    }
    Editor->TipControl = Control;
}

void Tooltip_Hide(TSoundfontPlug *Plug)
{
    PEditor Editor = &Plug->Editor;

    if (Editor->Tip && IsWindowVisible(Editor->Tip)) {
        ShowWindow(Editor->Tip, SW_HIDE);
    }
    Editor->TipControl = Tooltip_NoControl;
}

void Tooltip_Free(TSoundfontPlug *Plug)
{
    if (Plug->Editor.Tip) {
        DestroyWindow(Plug->Editor.Tip);
        Plug->Editor.Tip = NULL;
    }
}
