#include "fsp_cursors.h"

#include <string.h>

const i32 Cursor_ArtSize = 16;
const i32 Cursor_Size = 32;
const i32 Cursor_HotSpot = 8;
const i32 Cursor_BitsPerByte = 8;
const i32 Cursor_RowBytes = Cursor_Size / Cursor_BitsPerByte;
const u8 Cursor_LeftBit = 0x80;
const u8 Cursor_Transparent = 0xff;
const c8 Cursor_Clear = ' ';
const c8 Cursor_White = '.';

const c8 *const Cursor_PalmArt[Cursor_ArtSize] = {
    "       XX       ",
    "   XX X..XXX    ",
    "  X..XX..X..X   ",
    "  X..XX..X..X X ",
    "   X..X..X..XX.X",
    "   X..X..X..X..X",
    " XX X........X.X",
    "X..XX..........X",
    "X...X.........X ",
    " X............X ",
    "  X...........X ",
    "  X..........X  ",
    "   X.........X  ",
    "    X.......X   ",
    "     X......X   ",
    "     X......X   ",
};

const c8 *const Cursor_GrabArt[Cursor_ArtSize] = {
    "                ",
    "                ",
    "                ",
    "    XX XX XX    ",
    "   X..X..X..XX  ",
    "   X........X.X ",
    "    X.........X ",
    "   XX.........X ",
    "  X...........X ",
    "  X...........X ",
    "  X..........X  ",
    "   X.........X  ",
    "    X.......X   ",
    "     X......X   ",
    "     X......X   ",
    "                ",
};

static HCURSOR Cursor_PalmHandle;
static HCURSOR Cursor_GrabHandle;

static HCURSOR Cursors_Build(HINSTANCE Instance, const c8 *const Art[Cursor_ArtSize])
{
    u8 AndMask[Cursor_Size * Cursor_RowBytes];
    u8 XorMask[Cursor_Size * Cursor_RowBytes];

    memset(AndMask, Cursor_Transparent, sizeof(AndMask));
    memset(XorMask, 0, sizeof(XorMask));
    for (i32 Y = 0; Y < Cursor_ArtSize; ++Y) {
        for (i32 X = 0; X < Cursor_ArtSize; ++X) {
            c8 Mark = Art[Y][X];
            i32 Byte = Y * Cursor_RowBytes + X / Cursor_BitsPerByte;
            u8 Bit = (u8)(Cursor_LeftBit >> (X % Cursor_BitsPerByte));

            if (Mark == Cursor_Clear) {
                continue;
            }
            AndMask[Byte] &= (u8)~Bit;
            if (Mark == Cursor_White) {
                XorMask[Byte] |= Bit;
            }
        }
    }
    return CreateCursor(Instance, Cursor_HotSpot, Cursor_HotSpot, Cursor_Size, Cursor_Size, AndMask, XorMask);
}

void Cursors_Create(HINSTANCE Instance)
{
    Cursor_PalmHandle = Cursors_Build(Instance, Cursor_PalmArt);
    Cursor_GrabHandle = Cursors_Build(Instance, Cursor_GrabArt);
}

HCURSOR Cursors_Palm()
{
    return Cursor_PalmHandle;
}

HCURSOR Cursors_Grab()
{
    return Cursor_GrabHandle;
}
