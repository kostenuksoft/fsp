# fsp

Fruity Soundfont Player from FL Studio 11, rebuilt so it loads in the 64-bit version of FL. There is a 32-bit build as well.

The sound still comes from the original LiveSynth Pro engine. On x64 the engine is statically recompiled from the original `FLSPro.dll` and compiled into the plugin. On x32 the plugin loads that DLL as it always did. The editor is redrawn from the original artwork. It also has a list of recent soundfonts, a patch explorer you can search, menus that look like FL's own, value tooltips, and a prompt for typing an exact value.

Only the code written for this rebuild is in the repository. Image-Line's files are not, so you have to bring a few things yourself before it builds.

## Requirements

- FL Studio 11 with the original Fruity Soundfont Player installed. At run time the plugin uses three of its files: `FLSPro.dll`, `Artwork\Back.bmp`, and the original plugin DLL. The last one decides whether the plugin runs registered or as a trial.
- MinGW-w64 (GCC) for the x64 build.
- Visual Studio with the C++ x86 tools for the x32 build.
- Python 3 for the generators and the tests.

## What you have to supply

| Needed | Where it goes | Where it comes from |
| --- | --- | --- |
| Image-Line FL Studio plugin SDK: `fp_plugclass.h`, `fp_plugclass.cpp`, `fp_def.h` | `plugin/sdk/` | Image-Line |
| the original `FLSPro.dll` (x64 only) | `re/orig/FLSPro.dll`, or set `FLSPRO_DLL` to its path | your FL Studio 11 installation |
| the recompiled engine sources (x64 only) | `engine/gen/` | `sh build.sh regen` runs `engine/lift.py` on a p-code dump of your `FLSPro.dll`, which it expects at `re/out/FLSPro.dll.pcode.jsonl` |
| the editor image data: `fsp_glyphs.h`, `fsp_glyphs.cpp` | `plugin/` | decoded from the original plugin's image strips |

If one of these is missing, the build script stops and tells you which.

## Building

```
sh build.sh
build32.bat
```

The DLLs land in `dist/`:

| File | Architecture |
| --- | --- |
| `Fruity Soundfont Player_x64.dll` | x64 |
| `Fruity Soundfont Player.dll` | x32 |

Object files and other leftovers go to `build/`. You can delete that folder whenever you like.

Add `tests` to either command to build the test programs too: `sh build.sh tests`, `build32.bat tests`.

## Installing

1. In the FL Studio folder, open `Plugins\Fruity\Generators\Fruity Soundfont Player`.
2. Rename the original `Fruity Soundfont Player.dll` to `Fruity Soundfont Player (original).dll`.
3. Copy both DLLs from `dist/` into that folder.

To undo this, delete the two DLLs you copied and give the original its name back.

## Layout

| Path | Contents |
| --- | --- |
| `common/` | type aliases shared by the C and C++ code |
| `engine/` | the runtime the recompiled engine runs on, the engine API, the recompiler (`lift.py`) and the image generator (`make_image.py`) |
| `plugin/` | the plugin itself: the FL SDK object, editor, patch explorer and menus |
| `tests/` | a stand-in host, and a comparison of the recompiled engine against the original |
| `re/engine/` | decompiled listings of the functions `FLSPro.dll` exports, one file per function |
| `re/plugin/` | decompiled listings of the functions identified in the original plugin, one file per function |

## Testing

```
sh build.sh tests
sh tests/run_diff.sh <soundfont.sf2> <seed>...
```

For each seed, `run_diff.sh` generates a random script of notes and parameter changes and renders it twice, once through the recompiled engine and once through the original `FLSPro.dll`. Then it compares the two renders byte for byte. The original DLL has to be at `re/orig/FLSPro.dll`.

`build/x64/host_test.exe <plugin dll> <soundfont.sf2> [search text]` loads a built plugin in a minimal host. It plays a note, saves and restores the plugin's state, and writes screenshots of the editor and the patch explorer into the current directory.

## License

GPL-3.0, see `LICENSE`.
