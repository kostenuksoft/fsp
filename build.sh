#!/bin/sh
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

OBJ="build/x64"
ENGINE_FLAGS="-std=gnu11 -O2 -fno-strict-aliasing -frounding-math -fwrapv -w"
PLUGIN_FLAGS="-std=gnu++17 -O2 -fno-exceptions -fno-rtti -w"
PLUGIN_LIBS="-lwinmm -lcomdlg32 -lgdi32 -lshell32 -lole32 -ladvapi32 -lcomctl32"
ENGINE_SOURCES="gen/lifted_0 gen/lifted_1 gen/lifted_2 gen/lifted_3 gen/lifted_4 gen/lifted_5 gen/dispatch gen/image vm flspro"

mkdir -p "$OBJ" dist

ORIGINAL_ENGINE="${FLSPRO_DLL:-re/orig/FLSPro.dll}"
MISSING=0

require() {
    if [ ! -e "$1" ]; then
        echo "missing $1"
        echo "    $2"
        MISSING=1
    fi
}

if [ "$1" = "regen" ]; then
    python engine/lift.py
    rm -f engine/gen/image.c
fi

require plugin/sdk/fp_plugclass.h "copy the Image-Line FL Studio plugin SDK (fp_plugclass.h, fp_plugclass.cpp, fp_def.h) into plugin/sdk"
require engine/gen/dispatch.c "generate the engine sources with: sh build.sh regen (see README.md)"
require plugin/fsp_glyphs.cpp "the editor image data is not part of this repository (see README.md)"
if [ ! -f engine/gen/image.c ]; then
    require "$ORIGINAL_ENGINE" "copy the original FLSPro.dll to re/orig/FLSPro.dll or set FLSPRO_DLL to its path"
fi
if [ "$MISSING" = "1" ]; then
    exit 2
fi

python plugin/make_regprobe.py
if [ ! -f engine/gen/image.c ]; then
    python engine/make_image.py "$ORIGINAL_ENGINE"
fi

ENGINE_OBJECTS=""
JOBS=""
for source in $ENGINE_SOURCES; do
    object="$OBJ/$(basename "$source").o"
    gcc $ENGINE_FLAGS -c "engine/$source.c" -o "$object" &
    JOBS="$JOBS $!"
    ENGINE_OBJECTS="$ENGINE_OBJECTS $object"
done
for job in $JOBS; do
    wait "$job"
done

g++ $PLUGIN_FLAGS -shared -static -o "dist/Fruity Soundfont Player_x64.dll" plugin/fsp_*.cpp plugin/sdk/fp_plugclass.cpp \
    plugin/fsp.def $ENGINE_OBJECTS $PLUGIN_LIBS

if [ "$1" = "tests" ]; then
    gcc $ENGINE_FLAGS -static -o "$OBJ/script_test.exe" tests/script_test.c $ENGINE_OBJECTS -lwinmm
    g++ -std=gnu++17 -O1 -w -static -o "$OBJ/host_test.exe" tests/host_test.cpp -lgdi32 -lole32
fi
