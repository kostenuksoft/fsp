#include "fsp_explorer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsp_gdi.h"
#include "fsp_main.h"
#include "fsp_paint.h"
#include "fsp_patches.h"
#include "fsp_plugclass.h"
#include "fsp_theme.h"

const c8 Explorer_Title[] = "Patches";
const c8 Explorer_Prompt[] = "type to search...";
const c8 Explorer_Caret[] = "_";
const c8 Explorer_CountFormat[] = "%d / %d";
const c8 Explorer_NumberFormat[] = "%03d";
const c8 Explorer_DrumBankLabel[] = "DRM";
const c8 Explorer_SearchFormat[] = "%03d %03d %s";
const u32 Explorer_QuerySize = 33;
const u32 Explorer_CountSize = 64;
const i32 Explorer_WheelRows = 3;
const WPARAM Explorer_Backspace = 8;
const WPARAM Explorer_FirstPrintable = 32;
const WPARAM Explorer_SymbolCount = 256;
const i32 Explorer_ArrowPoints = 3;

enum {
    Sort_ByPatch,
    Sort_ByBank,
    Sort_ByName
};

enum {
    Row_None = -1,
    Row_Scrolling = -2
};

typedef struct {
    HWND Window;
    TSoundfontPlug *Plug;
    PPatchList Patches;
    u16 Shown[Patch_MaxCount];
    i32 ShownCount;
    c8 Query[Explorer_QuerySize];
    i32 Selected;
    i32 Hover;
    i32 Scroll;
    bool Done;
} TExplorer, *PExplorer;

static TExplorer Explorer;

static const TPatchEntry *Explorer_Entry(i32 Row)
{
    return &Explorer.Patches->Entries[Explorer.Shown[Row]];
}

static i32 Explorer_SortMode()
{
    return Explorer.Plug->Editor.ExplorerSortMode;
}

static bool Explorer_SortDescending()
{
    return Explorer.Plug->Editor.ExplorerSortDescending;
}

static i32 Explorer_Order(const TPatchEntry *Left, const TPatchEntry *Right, i32 Mode)
{
    i32 ByName = Mode == Sort_ByName ? _stricmp(Left->Name, Right->Name) : 0;

    if (ByName != 0) {
        return ByName;
    }
    if (Mode == Sort_ByBank && Left->Bank != Right->Bank) {
        return Left->Bank - Right->Bank;
    }
    if (Left->Patch != Right->Patch) {
        return Left->Patch - Right->Patch;
    }
    return Left->Bank - Right->Bank;
}

static int Explorer_Compare(const void *Left, const void *Right)
{
    const TPatchEntry *First = &Explorer.Patches->Entries[*(const u16 *)Left];
    const TPatchEntry *Second = &Explorer.Patches->Entries[*(const u16 *)Right];

    if (Explorer_SortDescending()) {
        return Explorer_Order(Second, First, Explorer_SortMode());
    }
    return Explorer_Order(First, Second, Explorer_SortMode());
}

static bool Explorer_Matches(const TPatchEntry *Entry, const c8 *Needle)
{
    c8 Label[Text_LabelSize];

    if (Text_IsEmpty(Needle)) {
        return true;
    }
    snprintf(Label, sizeof(Label), Explorer_SearchFormat, Entry->Patch + 1, Entry->Bank, Entry->Name);
    Text_Lower(Label);
    return Text_Contains(Label, Needle);
}

static i32 Explorer_MaxScroll()
{
    return Explorer.ShownCount > Layout_ExplorerRows ? Explorer.ShownCount - Layout_ExplorerRows : 0;
}

static void Explorer_Apply(bool Close)
{
    i32 Row = Explorer.Selected;

    if (Row < 0 && Close && Explorer.ShownCount > 0) {
        Row = 0;
    }
    if (Row >= 0 && Row < Explorer.ShownCount) {
        Plug_SelectPatch(Explorer.Plug, Explorer_Entry(Row)->Bank, Explorer_Entry(Row)->Patch);
    }
    if (Close && Explorer.Window) {
        PostMessageA(Explorer.Window, WM_CLOSE, 0, 0);
    }
}

static void Explorer_Select(i32 Row, bool Apply)
{
    if (Row >= Explorer.ShownCount) {
        Row = Explorer.ShownCount - 1;
    }
    if (Row >= 0) {
        Explorer.Selected = Row;
        if (Row < Explorer.Scroll) {
            Explorer.Scroll = Row;
        }
        if (Row >= Explorer.Scroll + Layout_ExplorerRows) {
            Explorer.Scroll = Row - Layout_ExplorerRows + 1;
        }
    }
    if (Explorer.Scroll > Explorer_MaxScroll()) {
        Explorer.Scroll = Explorer_MaxScroll();
    }
    if (Explorer.Scroll < 0) {
        Explorer.Scroll = 0;
    }
    if (Apply && Row >= 0) {
        Explorer_Apply(false);
    }
    if (Explorer.Window) {
        InvalidateRect(Explorer.Window, NULL, FALSE);
    }
}

static i32 Explorer_FindRow(i32 KeptEntry)
{
    i32 Found = Row_None;

    for (i32 Row = 0; Row < Explorer.ShownCount; ++Row) {
        const TPatchEntry *Entry = Explorer_Entry(Row);
        bool Current = Entry->Bank == Plug_CurrentBank(Explorer.Plug) && Entry->Patch == Explorer.Plug->Program;

        if (KeptEntry >= 0 ? Explorer.Shown[Row] == KeptEntry : Current) {
            Found = Row;
        }
    }
    return Found;
}

static void Explorer_Fill()
{
    c8 Needle[Explorer_QuerySize];
    bool HadSelection = Explorer.Selected >= 0 && Explorer.Selected < Explorer.ShownCount;
    i32 KeptEntry = HadSelection ? Explorer.Shown[Explorer.Selected] : Row_None;

    Text_Copy(Needle, sizeof(Needle), Explorer.Query);
    Text_Lower(Needle);
    Explorer.ShownCount = 0;
    for (u32 Index = 0; Index < Explorer.Patches->Count; ++Index) {
        if (Explorer_Matches(&Explorer.Patches->Entries[Index], Needle)) {
            Explorer.Shown[Explorer.ShownCount] = (u16)Index;
            ++Explorer.ShownCount;
        }
    }
    qsort(Explorer.Shown, (usize)Explorer.ShownCount, sizeof(Explorer.Shown[0]), Explorer_Compare);
    Explorer.Selected = Explorer_FindRow(KeptEntry);
    Explorer.Scroll = Explorer.Selected >= 0 ? Explorer.Selected - Layout_ExplorerRows / 2 : 0;
    Explorer_Select(Row_None, false);
}

static i32 Explorer_TitleAt(i32 X, i32 Y)
{
    if (Y < Layout_ExplorerTitlesTop || Y >= Layout_ExplorerList.Y) {
        return Row_None;
    }
    for (i32 Column = 0; Column < Layout_ExplorerSortColumns; ++Column) {
        if (X < Layout_ExplorerTitleRight[Column]) {
            return Column;
        }
    }
    return Row_None;
}

static i32 Explorer_RowAt(i32 X, i32 Y)
{
    i32 Offset = Y - Layout_ExplorerList.Y - Layout_ExplorerListPad;
    i32 Row;

    if (Y >= Layout_ExplorerTitlesTop && Y < Layout_ExplorerList.Y) {
        return Row_None;
    }
    if (X < Layout_ExplorerList.X || X >= Layout_ExplorerHitRight) {
        return Row_None;
    }
    if (Offset < 0 || Offset >= Layout_ExplorerRows * Layout_ExplorerRowH) {
        return Row_None;
    }
    Row = Explorer.Scroll + Offset / Layout_ExplorerRowH;
    return Row < Explorer.ShownCount ? Row : Row_None;
}

static bool Explorer_OnTrack(i32 X, i32 Y)
{
    const TBox *List = &Layout_ExplorerList;

    return X >= Layout_ExplorerRowRight && X < List->X + List->W && Y >= List->Y && Y < List->Y + List->H;
}

static void Explorer_PaintBackground(HDC DC, HDC Artwork, bool HasArtwork)
{
    TBox Whole = { 0, 0, Layout_ExplorerW, Layout_ExplorerH };
    RECT Outline = { 0, 0, Layout_ExplorerW, Layout_ExplorerH };
    TGdiObject Edge = { CreateSolidBrush(Theme_ExplorerEdge) };

    if (HasArtwork) {
        Paint_ArtworkStretch(DC, Artwork, &Whole, &Artwork_PanelFill);
        Paint_ArtworkCopy(DC, Artwork, 0, 0, &Artwork_PanelTopEdge);
        Paint_ArtworkCopy(DC, Artwork, 0, Layout_ExplorerH - Artwork_PanelBottomEdge.H, &Artwork_PanelBottomEdge);
        Paint_ArtworkCopy(DC, Artwork, Layout_ExplorerTitles.X, Layout_ExplorerTitles.Y, &Artwork_ColumnTitles);
        Paint_Frame(DC, Artwork, &Layout_ExplorerSearch, Layout_ExplorerSearch.W / 2);
        Paint_Frame(DC, Artwork, &Layout_ExplorerList, Layout_ExplorerList.W / 2);
    } else {
        Paint_Fill(DC, &Whole, Theme_ExplorerFallback);
        Paint_Fill(DC, &Layout_ExplorerSearch, Theme_ExplorerWell);
        Paint_Fill(DC, &Layout_ExplorerList, Theme_ExplorerWell);
    }
    FrameRect(DC, &Outline, (HBRUSH)Edge.Handle);
}

static void Explorer_PaintSearch(HDC DC)
{
    c8 Count[Explorer_CountSize];
    c8 Typed[Explorer_QuerySize + sizeof(Explorer_Caret)];
    bool Empty = Text_IsEmpty(Explorer.Query);

    snprintf(Count, sizeof(Count), Explorer_CountFormat, Explorer.ShownCount, (i32)Explorer.Patches->Count);
    Paint_Text(DC, &Layout_ExplorerCountText, Count, Theme_TextDim, DT_RIGHT);
    Text_Copy(Typed, sizeof(Typed), Explorer.Query);
    Text_Append(Typed, sizeof(Typed), Explorer_Caret);
    Paint_Text(DC, &Layout_ExplorerQueryText, Empty ? Explorer_Prompt : Typed, Empty ? Theme_TextDim : Theme_Accent,
               DT_LEFT);
}

static void Explorer_PaintSortMark(HDC DC)
{
    const TBox *Arrow = &Layout_ExplorerSortArrow;
    TGdiObject Accent = { CreateSolidBrush(Theme_Accent) };
    TSelection Brush = { DC, SelectObject(DC, Accent.Handle) };
    TSelection Pen = { DC, SelectObject(DC, GetStockObject(NULL_PEN)) };
    i32 BaseY = Explorer_SortDescending() ? Arrow->Y : Arrow->Y + Arrow->H;
    i32 TipY = Explorer_SortDescending() ? Arrow->Y + Arrow->H : Arrow->Y;
    POINT Points[Explorer_ArrowPoints] = {
        { Arrow->X, BaseY },
        { Arrow->X + Arrow->W, BaseY },
        { Arrow->X + Arrow->W / 2, TipY },
    };

    Paint_Fill(DC, &Layout_ExplorerSortMarks[Explorer_SortMode()], Theme_Accent);
    Polygon(DC, Points, Explorer_ArrowPoints);
}

static void Explorer_PaintRow(HDC DC, i32 Line, i32 Row)
{
    const TPatchEntry *Entry = Explorer_Entry(Row);
    i32 Y = Layout_ExplorerList.Y + Layout_ExplorerListPad + Line * Layout_ExplorerRowH;
    bool Selected = Row == Explorer.Selected;
    COLORREF NumberInk = Selected ? Theme_Accent : Theme_ExplorerNumber;
    TBox Highlight = { Layout_ExplorerRowLeft, Y, Layout_ExplorerRowRight - Layout_ExplorerRowLeft,
                       Layout_ExplorerRowH };
    TBox PatchBox = { Layout_ExplorerPatchX, Y, Layout_ExplorerNumberW, Layout_ExplorerRowH };
    TBox BankBox = { Layout_ExplorerBankX, Y, Layout_ExplorerNumberW, Layout_ExplorerRowH };
    TBox NameBox = { Layout_ExplorerNameX, Y, Layout_ExplorerNameW, Layout_ExplorerRowH };
    c8 Number[Text_NumberSize];
    c8 Bank[Text_NumberSize];

    if (Selected || Row == Explorer.Hover) {
        Paint_Fill(DC, &Highlight, Selected ? Theme_ExplorerSelected : Theme_ExplorerHover);
    }
    snprintf(Number, sizeof(Number), Explorer_NumberFormat, Entry->Patch + 1);
    if (Entry->Bank == Midi_DrumBank) {
        Text_Copy(Bank, sizeof(Bank), Explorer_DrumBankLabel);
    } else {
        snprintf(Bank, sizeof(Bank), Explorer_NumberFormat, Entry->Bank);
    }
    Paint_Text(DC, &PatchBox, Number, NumberInk, DT_LEFT);
    Paint_Text(DC, &BankBox, Bank, NumberInk, DT_LEFT);
    Paint_Text(DC, &NameBox, Entry->Name, Selected ? Theme_Accent : Theme_TextBright, DT_LEFT);
}

static void Explorer_PaintScrollBar(HDC DC)
{
    i32 ThumbH = Layout_ExplorerTrackLen * Layout_ExplorerRows / Explorer.ShownCount;
    TGdiObject Edge = { CreateSolidBrush(Theme_ExplorerEdge) };
    TBox Track = { Layout_ExplorerTrackLineX, Layout_ExplorerTrackTop, 1, Layout_ExplorerTrackLen };
    TBox Thumb;
    TBox Grip;
    RECT ThumbOutline;

    if (ThumbH < Layout_ExplorerThumbMinH) {
        ThumbH = Layout_ExplorerThumbMinH;
    }
    Thumb.X = Layout_ExplorerTrackX;
    Thumb.Y = Layout_ExplorerTrackTop + (Layout_ExplorerTrackLen - ThumbH) * Explorer.Scroll / Explorer_MaxScroll();
    Thumb.W = Layout_ExplorerThumbW;
    Thumb.H = ThumbH;
    Grip = Thumb;
    Grip.Y += ThumbH / 2;
    Grip.H = 1;
    SetRect(&ThumbOutline, Thumb.X, Thumb.Y, Thumb.X + Thumb.W, Thumb.Y + Thumb.H);
    Paint_Fill(DC, &Track, Theme_ExplorerTrack);
    Paint_Fill(DC, &Thumb, Theme_ExplorerThumb);
    FrameRect(DC, &ThumbOutline, (HBRUSH)Edge.Handle);
    Paint_Fill(DC, &Grip, Theme_Accent);
}

static void Explorer_PaintAll(HDC DC)
{
    HBITMAP Bitmap = Editor_Artwork(Explorer.Plug);
    TMemoryDC Artwork = { CreateCompatibleDC(DC) };
    TSelection Selected = { Artwork.Handle, Bitmap ? SelectObject(Artwork.Handle, Bitmap) : NULL };
    TSelection Font = { DC, NULL };

    Explorer_PaintBackground(DC, Artwork.Handle, Bitmap != NULL);
    SetBkMode(DC, TRANSPARENT);
    Font.Previous = SelectObject(DC, Paint_PanelFont());
    Explorer_PaintSearch(DC);
    Explorer_PaintSortMark(DC);
    for (i32 Line = 0; Line < Layout_ExplorerRows && Explorer.Scroll + Line < Explorer.ShownCount; ++Line) {
        Explorer_PaintRow(DC, Line, Explorer.Scroll + Line);
    }
    if (Explorer.ShownCount > Layout_ExplorerRows) {
        Explorer_PaintScrollBar(DC);
    }
}

static void Explorer_OnPaint(HWND Window)
{
    PAINTSTRUCT Paint;
    HDC DC = BeginPaint(Window, &Paint);
    TMemoryDC Buffer = { CreateCompatibleDC(DC) };
    TGdiObject Bitmap = { CreateCompatibleBitmap(DC, Layout_ExplorerW, Layout_ExplorerH) };
    TSelection Selected = { Buffer.Handle, SelectObject(Buffer.Handle, Bitmap.Handle) };

    Explorer_PaintAll(Buffer.Handle);
    BitBlt(DC, 0, 0, Layout_ExplorerW, Layout_ExplorerH, Buffer.Handle, 0, 0, SRCCOPY);
    EndPaint(Window, &Paint);
}

static void Explorer_DragScroll(i32 Y)
{
    i32 Length = Layout_ExplorerTrackLen > 1 ? Layout_ExplorerTrackLen : 1;

    Explorer.Scroll = (Y - Layout_ExplorerTrackTop) * Explorer_MaxScroll() / Length;
    Explorer_Select(Row_None, false);
}

static void Explorer_OnMouseMove(HWND Window, WPARAM Keys, i32 X, i32 Y)
{
    i32 Row;

    if (Explorer.Hover == Row_Scrolling && (Keys & MK_LBUTTON) && Explorer.ShownCount > Layout_ExplorerRows) {
        Explorer_DragScroll(Y);
        return;
    }
    Row = Explorer_RowAt(X, Y);
    if (Row != Explorer.Hover) {
        Explorer.Hover = Row;
        InvalidateRect(Window, NULL, FALSE);
    }
}

static void Explorer_SortBy(i32 Column)
{
    PEditor Editor = &Explorer.Plug->Editor;

    if (Editor->ExplorerSortMode == Column) {
        Editor->ExplorerSortDescending = !Editor->ExplorerSortDescending;
    } else {
        Editor->ExplorerSortMode = Column;
        Editor->ExplorerSortDescending = false;
    }
    Explorer_Fill();
}

static void Explorer_OnClick(HWND Window, UINT Message, LPARAM LParam, i32 X, i32 Y)
{
    i32 Title = Explorer_TitleAt(X, Y);
    i32 Row = Explorer_RowAt(X, Y);

    if (Explorer_OnTrack(X, Y)) {
        Explorer.Hover = Row_Scrolling;
        SetCapture(Window);
        SendMessageA(Window, WM_MOUSEMOVE, MK_LBUTTON, LParam);
        return;
    }
    if (Title >= 0) {
        Explorer_SortBy(Title);
        return;
    }
    if (Row >= 0) {
        Explorer_Select(Row, true);
        if (Message == WM_LBUTTONDBLCLK) {
            Explorer_Apply(true);
        }
    }
}

static void Explorer_OnKey(HWND Window, WPARAM Key)
{
    i32 Selected = Explorer.Selected;

    switch (Key) {
    case VK_ESCAPE:
        PostMessageA(Window, WM_CLOSE, 0, 0);
        break;
    case VK_RETURN:
        Explorer_Apply(true);
        break;
    case VK_DOWN:
        Explorer_Select(Selected < 0 ? 0 : Selected + 1, true);
        break;
    case VK_UP:
        Explorer_Select(Selected <= 0 ? 0 : Selected - 1, true);
        break;
    case VK_NEXT:
        Explorer_Select(Selected < 0 ? 0 : Selected + Layout_ExplorerRows, true);
        break;
    case VK_PRIOR:
        Explorer_Select(Selected < Layout_ExplorerRows ? 0 : Selected - Layout_ExplorerRows, true);
        break;
    case VK_HOME:
        Explorer_Select(0, true);
        break;
    case VK_END:
        Explorer_Select(Explorer.ShownCount - 1, true);
        break;
    }
}

static void Explorer_OnChar(WPARAM Symbol)
{
    usize Length = strlen(Explorer.Query);

    if (Symbol == Explorer_Backspace) {
        if (Length > 0) {
            Explorer.Query[Length - 1] = 0;
        }
    } else if (Symbol >= Explorer_FirstPrintable && Symbol < Explorer_SymbolCount && Length + 1 < Explorer_QuerySize) {
        Explorer.Query[Length] = (c8)Symbol;
        Explorer.Query[Length + 1] = 0;
    } else {
        return;
    }
    Explorer.Selected = Row_None;
    Explorer_Fill();
}

static LRESULT CALLBACK Explorer_WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    i32 X = (i16)LOWORD(LParam);
    i32 Y = (i16)HIWORD(LParam);

    if (!Explorer.Plug || (Explorer.Window && Explorer.Window != Window)) {
        return DefWindowProcA(Window, Message, WParam, LParam);
    }
    switch (Message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Explorer_OnPaint(Window);
        return 0;
    case WM_MOUSEMOVE:
        Explorer_OnMouseMove(Window, WParam, X, Y);
        return 0;
    case WM_LBUTTONUP:
        if (Explorer.Hover == Row_Scrolling) {
            Explorer.Hover = Row_None;
            ReleaseCapture();
        }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        Explorer_OnClick(Window, Message, LParam, X, Y);
        return 0;
    case WM_MOUSEWHEEL:
        Explorer.Scroll += (i16)HIWORD(WParam) > 0 ? -Explorer_WheelRows : Explorer_WheelRows;
        Explorer_Select(Row_None, false);
        return 0;
    case WM_KEYDOWN:
        Explorer_OnKey(Window, WParam);
        return 0;
    case WM_CHAR:
        Explorer_OnChar(WParam);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(WParam) == WA_INACTIVE) {
            PostMessageA(Window, WM_CLOSE, 0, 0);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(Window);
        return 0;
    case WM_DESTROY:
        Explorer.Window = NULL;
        Explorer.Done = true;
        return 0;
    }
    return DefWindowProcA(Window, Message, WParam, LParam);
}

void Explorer_Startup(HINSTANCE Instance)
{
    WNDCLASSA Class;

    memset(&Class, 0, sizeof(Class));
    Class.style = CS_DBLCLKS;
    Class.lpfnWndProc = Explorer_WindowProc;
    Class.hInstance = Instance;
    Class.hCursor = LoadCursor(NULL, IDC_ARROW);
    Class.lpszClassName = Explorer_ClassName;
    RegisterClassA(&Class);
}

void Explorer_Shutdown(HINSTANCE Instance)
{
    UnregisterClassA(Explorer_ClassName, Instance);
}

void Explorer_OwnerDestroyed(TSoundfontPlug *Plug)
{
    if (Explorer.Plug != Plug) {
        return;
    }
    if (Explorer.Window) {
        DestroyWindow(Explorer.Window);
    }
    Explorer.Plug = NULL;
    Explorer.Done = true;
}

static POINT Explorer_Placement(HWND EditorWindow)
{
    RECT Editor;
    RECT WorkArea;
    POINT Spot;

    GetWindowRect(EditorWindow, &Editor);
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &WorkArea, 0);
    Spot.x = Editor.left;
    Spot.y = Editor.top + Layout_ExplorerBelowEditorY;
    if (Spot.x + Layout_ExplorerW > WorkArea.right) {
        Spot.x = WorkArea.right - Layout_ExplorerW;
    }
    if (Spot.y + Layout_ExplorerH > WorkArea.bottom) {
        Spot.y = Editor.top + Layout_ExplorerAboveEditorY - Layout_ExplorerH;
    }
    if (Spot.x < WorkArea.left) {
        Spot.x = WorkArea.left;
    }
    if (Spot.y < WorkArea.top) {
        Spot.y = WorkArea.top;
    }
    return Spot;
}

static void Explorer_RunModal()
{
    MSG Message;

    while (!Explorer.Done) {
        BOOL Got = GetMessageA(&Message, NULL, 0, 0);

        if (Got == 0) {
            PostQuitMessage((int)Message.wParam);
        }
        if (Got <= 0) {
            break;
        }
        TranslateMessage(&Message);
        DispatchMessageA(&Message);
    }
}

void Explorer_Open(TSoundfontPlug *Plug)
{
    POINT Spot;

    if (!Plug_HasSynth(Plug) || Text_IsEmpty(Plug->FileName) || Explorer.Window) {
        return;
    }
    Explorer.Plug = Plug;
    Explorer.Patches = Patches_Scan(Plug->SynthInstance);
    Explorer.ShownCount = 0;
    Explorer.Query[0] = 0;
    Explorer.Selected = Row_None;
    Explorer.Hover = Row_None;
    Explorer.Scroll = 0;
    Explorer.Done = false;
    Spot = Explorer_Placement(Plug->EditorHandle);
    Explorer.Window = CreateWindowExA(WS_EX_TOOLWINDOW, Explorer_ClassName, Explorer_Title, WS_POPUP, Spot.x, Spot.y,
                                      Layout_ExplorerW, Layout_ExplorerH, GetAncestor(Plug->EditorHandle, GA_ROOT),
                                      NULL, Main_Instance(), NULL);
    if (!Explorer.Window) {
        Explorer.Plug = NULL;
        return;
    }
    Explorer_Fill();
    ShowWindow(Explorer.Window, SW_SHOW);
    SetFocus(Explorer.Window);
    Explorer_RunModal();
    if (!Explorer.Plug) {
        return;
    }
    if (Explorer.Window) {
        DestroyWindow(Explorer.Window);
    }
    Explorer.Plug = NULL;
    Editor_Redraw(Plug);
}
