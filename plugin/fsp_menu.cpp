#include "fsp_menu.h"

#include <string.h>

#include "fsp_gdi.h"
#include "fsp_patches.h"
#include "fsp_text.h"
#include "fsp_theme.h"

const u32 Menu_MaxItems = 2 * Patch_MaxCount;
const u32 Menu_AverageTextSize = 32;
const u32 Menu_TextPoolSize = Menu_MaxItems * Menu_AverageTextSize;

enum {
    Menu_Separator = 4,
    Menu_HasSubMenu = 8
};

const i32 Menu_SeparatorW = 40;
const i32 Menu_SeparatorH = 7;
const i32 Menu_SeparatorInset = 3;
const i32 Menu_ItemH = 19;
const i32 Menu_TextLeft = 18;
const i32 Menu_TextRightPad = 6;
const i32 Menu_HintGap = 18;
const i32 Menu_HintRightPad = 14;
const i32 Menu_CheckPenW = 2;
constexpr TSpot Menu_CheckStart = { 5, 0 };
constexpr TSpot Menu_CheckBend = { 8, 3 };
constexpr TSpot Menu_CheckEnd = { 13, -4 };
const i32 Menu_ArrowRight = 10;
const i32 Menu_ArrowColumns = 4;
const i32 Menu_ArrowHalfH = 3;

typedef struct {
    const c8 *Text;
    const c8 *Hint;
    u32 Flags;
} TMenuItem, *PMenuItem;

static TMenuItem Menu_Items[Menu_MaxItems];
static u32 Menu_ItemCount;
static c8 Menu_TextPool[Menu_TextPoolSize];
static u32 Menu_TextUsed;
static HBRUSH Menu_BackBrush;
static HFONT Menu_FontHandle;

HFONT Menu_Font()
{
    if (!Menu_FontHandle) {
        Menu_FontHandle = CreateFontA(Theme_MenuFontHeight, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                      DEFAULT_QUALITY, 0, Theme_FontName);
    }
    return Menu_FontHandle;
}

void Menu_Shutdown()
{
    if (Menu_BackBrush) {
        DeleteObject(Menu_BackBrush);
        Menu_BackBrush = NULL;
    }
    if (Menu_FontHandle) {
        DeleteObject(Menu_FontHandle);
        Menu_FontHandle = NULL;
    }
}

HMENU Menu_Create()
{
    HMENU Menu = CreatePopupMenu();
    MENUINFO Info;

    if (!Menu_BackBrush) {
        Menu_BackBrush = CreateSolidBrush(Theme_MenuBack);
    }
    memset(&Info, 0, sizeof(Info));
    Info.cbSize = sizeof(Info);
    Info.fMask = MIM_BACKGROUND;
    Info.hbrBack = Menu_BackBrush;
    SetMenuInfo(Menu, &Info);
    return Menu;
}

static const c8 *Menu_KeepText(const c8 *Text)
{
    usize Size = strlen(Text) + 1;
    c8 *Kept = &Menu_TextPool[Menu_TextUsed];

    if (Text_IsEmpty(Text) || Menu_TextUsed + Size > Menu_TextPoolSize) {
        return "";
    }
    memcpy(Kept, Text, Size);
    Menu_TextUsed += (u32)Size;
    return Kept;
}

static PMenuItem Menu_NewItem(const c8 *Text, const c8 *Hint, u32 Flags)
{
    PMenuItem Item;

    if (Menu_ItemCount == Menu_MaxItems) {
        return NULL;
    }
    Item = &Menu_Items[Menu_ItemCount];
    ++Menu_ItemCount;
    Item->Text = Menu_KeepText(Text);
    Item->Hint = Menu_KeepText(Hint);
    Item->Flags = Flags;
    return Item;
}

void Menu_AddItemWithHint(HMENU Menu, i32 Command, const c8 *Text, u32 Flags, const c8 *Hint)
{
    PMenuItem Item = Menu_NewItem(Text, Hint, Flags);
    UINT Style = MF_OWNERDRAW;

    if (!Item) {
        return;
    }
    if (Flags & Menu_Disabled) {
        Style |= MF_GRAYED;
    }
    AppendMenuA(Menu, Style, (UINT_PTR)Command, (LPCSTR)Item);
}

void Menu_AddItem(HMENU Menu, i32 Command, const c8 *Text, u32 Flags)
{
    Menu_AddItemWithHint(Menu, Command, Text, Flags, "");
}

void Menu_AddSubMenu(HMENU Menu, HMENU SubMenu, const c8 *Text, u32 Flags, const c8 *Hint)
{
    PMenuItem Item = Menu_NewItem(Text, Hint, Flags | Menu_HasSubMenu);

    if (!Item) {
        DestroyMenu(SubMenu);
        return;
    }
    AppendMenuA(Menu, MF_OWNERDRAW | MF_POPUP, (UINT_PTR)SubMenu, (LPCSTR)Item);
}

void Menu_AddSeparator(HMENU Menu)
{
    PMenuItem Item = Menu_NewItem("", "", Menu_Separator);

    if (!Item) {
        return;
    }
    AppendMenuA(Menu, MF_OWNERDRAW | MF_DISABLED, 0, (LPCSTR)Item);
}

i32 Menu_Track(HMENU Menu, POINT ScreenSpot, HWND Owner)
{
    UINT Flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    i32 Command = (i32)TrackPopupMenu(Menu, Flags, ScreenSpot.x, ScreenSpot.y, 0, Owner, NULL);

    DestroyMenu(Menu);
    Menu_ItemCount = 0;
    Menu_TextUsed = 0;
    return Command;
}

static i32 Menu_TextWidth(HDC DC, const c8 *Text, UINT Format)
{
    RECT Bounds = { 0, 0, 0, 0 };

    DrawTextA(DC, Text, -1, &Bounds, DT_SINGLELINE | DT_CALCRECT | Format);
    return Bounds.right;
}

bool Menu_Measure(MEASUREITEMSTRUCT *Measure)
{
    PMenuItem Item = (PMenuItem)Measure->itemData;
    TScreenDC Screen = { NULL };
    TSelection Font = { NULL, NULL };
    i32 Width;

    if (Measure->CtlType != ODT_MENU) {
        return false;
    }
    if (Item->Flags & Menu_Separator) {
        Measure->itemWidth = Menu_SeparatorW;
        Measure->itemHeight = Menu_SeparatorH;
        return true;
    }
    Screen.Handle = GetDC(NULL);
    Font.DC = Screen.Handle;
    Font.Previous = SelectObject(Screen.Handle, Menu_Font());
    Width = Menu_TextWidth(Screen.Handle, Item->Text, 0);
    if (!Text_IsEmpty(Item->Hint)) {
        Width += Menu_HintGap + Menu_TextWidth(Screen.Handle, Item->Hint, DT_NOPREFIX);
    }
    Measure->itemWidth = (UINT)(Width + Menu_TextLeft + Menu_HintRightPad);
    Measure->itemHeight = Menu_ItemH;
    return true;
}

static void Menu_Fill(HDC DC, const RECT *Area, COLORREF Color)
{
    TGdiObject Brush = { CreateSolidBrush(Color) };

    FillRect(DC, Area, (HBRUSH)Brush.Handle);
}

static void Menu_DrawSeparator(HDC DC, const RECT *Area)
{
    RECT Line = { Area->left + Menu_SeparatorInset, Area->top + Menu_SeparatorInset,
                  Area->right - Menu_SeparatorInset, Area->top + Menu_SeparatorInset + 1 };

    Menu_Fill(DC, &Line, Theme_MenuSeparator);
}

static void Menu_DrawCheck(HDC DC, const RECT *Area, COLORREF Ink)
{
    TGdiObject Pen = { CreatePen(PS_SOLID, Menu_CheckPenW, Ink) };
    TSelection Selected = { DC, SelectObject(DC, Pen.Handle) };
    i32 MiddleY = (Area->top + Area->bottom) / 2;

    MoveToEx(DC, Area->left + Menu_CheckStart.X, MiddleY + Menu_CheckStart.Y, NULL);
    LineTo(DC, Area->left + Menu_CheckBend.X, MiddleY + Menu_CheckBend.Y);
    LineTo(DC, Area->left + Menu_CheckEnd.X, MiddleY + Menu_CheckEnd.Y);
}

static void Menu_DrawArrow(HDC DC, const RECT *Area, COLORREF Color)
{
    i32 MiddleY = (Area->top + Area->bottom) / 2;
    i32 Left = Area->right - Menu_ArrowRight;

    for (i32 Column = 0; Column < Menu_ArrowColumns; ++Column) {
        RECT Strip = { Left + Column, MiddleY - Menu_ArrowHalfH + Column, Left + Column + 1,
                       MiddleY + Menu_ArrowHalfH + 1 - Column };

        Menu_Fill(DC, &Strip, Color);
    }
}

static void Menu_DrawTexts(HDC DC, const RECT *Area, const TMenuItem *Item, bool Hot, COLORREF Ink)
{
    RECT TextArea = { Area->left + Menu_TextLeft, Area->top, Area->right - Menu_TextRightPad, Area->bottom };
    RECT HintArea = { Area->left + Menu_TextLeft, Area->top, Area->right - Menu_HintRightPad, Area->bottom };

    SetBkMode(DC, TRANSPARENT);
    SetTextColor(DC, Ink);
    DrawTextA(DC, Item->Text, -1, &TextArea, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    if (Text_IsEmpty(Item->Hint)) {
        return;
    }
    SetTextColor(DC, Hot ? Theme_MenuHintInkHot : Theme_MenuHintInk);
    DrawTextA(DC, Item->Hint, -1, &HintArea, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX | DT_PATH_ELLIPSIS);
}

static COLORREF Menu_Ink(const TMenuItem *Item, bool Hot)
{
    if (Item->Flags & Menu_Disabled) {
        return Theme_MenuInkDisabled;
    }
    return Hot ? Theme_MenuInkHot : Theme_MenuInk;
}

static void Menu_DrawItem(HDC DC, const RECT *Area, const TMenuItem *Item, bool Hot)
{
    TSelection Font = { DC, SelectObject(DC, Menu_Font()) };
    COLORREF Ink = Menu_Ink(Item, Hot);

    Menu_DrawTexts(DC, Area, Item, Hot, Ink);
    if (Item->Flags & Menu_Checked) {
        Menu_DrawCheck(DC, Area, Ink);
    }
    if (Item->Flags & Menu_HasSubMenu) {
        Menu_DrawArrow(DC, Area, Hot ? Theme_MenuInkHot : Theme_MenuArrow);
    }
}

bool Menu_Draw(DRAWITEMSTRUCT *Draw)
{
    PMenuItem Item = (PMenuItem)Draw->itemData;
    HDC DC = Draw->hDC;
    RECT Area = Draw->rcItem;
    bool Hot;

    if (Draw->CtlType != ODT_MENU) {
        return false;
    }
    Hot = (Draw->itemState & ODS_SELECTED) && !(Item->Flags & (Menu_Disabled | Menu_Separator));
    Menu_Fill(DC, &Area, Hot ? Theme_MenuHot : Theme_MenuBack);
    if (Item->Flags & Menu_Separator) {
        Menu_DrawSeparator(DC, &Area);
        return true;
    }
    Menu_DrawItem(DC, &Area, Item, Hot);
    ExcludeClipRect(DC, Area.left, Area.top, Area.right, Area.bottom);
    return true;
}
