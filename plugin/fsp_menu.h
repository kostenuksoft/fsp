#ifndef FSP_MENU_H
#define FSP_MENU_H

#include "fsp_def.h"

enum {
    Menu_Plain = 0,
    Menu_Checked = 1,
    Menu_Disabled = 2
};

const i32 Menu_NoCommand = 0;

HMENU Menu_Create();
void Menu_AddItem(HMENU Menu, i32 Command, const c8 *Text, u32 Flags);
void Menu_AddItemWithHint(HMENU Menu, i32 Command, const c8 *Text, u32 Flags, const c8 *Hint);
void Menu_AddSubMenu(HMENU Menu, HMENU SubMenu, const c8 *Text, u32 Flags, const c8 *Hint);
void Menu_AddSeparator(HMENU Menu);
i32 Menu_Track(HMENU Menu, POINT ScreenSpot, HWND Owner);
bool Menu_Measure(MEASUREITEMSTRUCT *Measure);
bool Menu_Draw(DRAWITEMSTRUCT *Draw);
HFONT Menu_Font();
void Menu_Shutdown();

#endif
