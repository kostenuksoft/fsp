#!/bin/sh
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/build/work"
RUNNER="$ROOT/build/x64/script_test.exe"
ORIGINAL="$ROOT/re/orig/FLSPro.dll"
SHELL32="$(cygpath -u "$SYSTEMROOT")/SysWOW64/WindowsPowerShell/v1.0/powershell.exe"

if [ "$#" -lt 2 ]; then
    echo "usage: run_diff.sh <soundfont.sf2> <seed>..."
    exit 2
fi
if [ ! -x "$RUNNER" ]; then
    echo "missing $RUNNER, run: sh build.sh tests"
    exit 2
fi
if [ ! -f "$ORIGINAL" ]; then
    echo "missing $ORIGINAL, copy the original FLSPro.dll there"
    exit 2
fi

SOUNDFONT="$1"
shift
mkdir -p "$WORK"

identical=0
unstable=0
different=0

render_original() {
    "$SHELL32" -NoProfile -ExecutionPolicy Bypass -File "$ROOT/tests/oracle_script.ps1" \
        -Dll "$(cygpath -w "$ORIGINAL")" \
        -Script "$(cygpath -w "$WORK/s$1.txt")" \
        -Out "$(cygpath -w "$WORK/$2$1.f32")" \
        -Log "$(cygpath -w "$WORK/$2$1.log")" > "$WORK/$2$1.err" 2>&1 || true
}

compare() {
    python "$ROOT/tests/cmp.py" "$1" "$2" 2>&1 | tail -1
}

same_render() {
    cmp -s "$WORK/new$1.f32" "$WORK/ref$1.f32" && cmp -s "$WORK/new$1.log" "$WORK/ref$1.log" && [ -s "$WORK/ref$1.f32" ]
}

for seed in "$@"; do
    python "$ROOT/tests/gen_script.py" "$seed" "$(cygpath -w "$SOUNDFONT")" "$WORK/s$seed.txt"
    "$RUNNER" "$WORK/s$seed.txt" "$WORK/new$seed.f32" "$WORK/new$seed.log" 2> "$WORK/new$seed.err" || true
    render_original "$seed" ref
    notes="$(grep -c '^on .*=1$' "$WORK/new$seed.log" || true)"
    if same_render "$seed"; then
        identical=$((identical + 1))
        echo "seed $seed: IDENTICAL  notes=$notes  $(cat "$WORK/new$seed.err")"
        rm -f "$WORK/new$seed.f32" "$WORK/ref$seed.f32"
        continue
    fi
    render_original "$seed" refB
    if cmp -s "$WORK/refB$seed.f32" "$WORK/ref$seed.f32"; then
        different=$((different + 1))
        echo "seed $seed: DIFFERENT  notes=$notes  $(compare "$WORK/new$seed.f32" "$WORK/ref$seed.f32")"
        diff "$WORK/new$seed.log" "$WORK/ref$seed.log" | head -5 || true
    else
        unstable=$((unstable + 1))
        echo "seed $seed: ORIGINAL NOT REPRODUCIBLE  notes=$notes"
        echo "  new against original:      $(compare "$WORK/new$seed.f32" "$WORK/ref$seed.f32")"
        echo "  original against original: $(compare "$WORK/refB$seed.f32" "$WORK/ref$seed.f32")"
    fi
    rm -f "$WORK/refB$seed.f32"
done

echo "identical $identical, original-not-reproducible $unstable, different $different"
