#!/bin/sh
# Build NLarn for the browser (Emscripten + Asyncify) into web/dist.
# GLib: port/glib (a small GLib subset on libc), curses: port/curses.h +
# port/wcurses.c (in-memory curses with panels), frontend: port/be_web.c.
# Run with sh. ASAN=1 sh web/build.sh builds an AddressSanitizer variant.
set -e
cd "$(dirname "$0")/.."
command -v emcc >/dev/null 2>&1 || PATH="${EMSDK:-/home/user/emsdk}/upstream/emscripten:$PATH"
OUT=web/dist
rm -rf "$OUT" && mkdir -p "$OUT"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
# the game's data (English only; the page has no locale choice)
mkdir -p "$STAGE/lib"
cp lib/fortune lib/maze lib/nlarn.hlp lib/nlarn.msg "$STAGE/lib/"
VERSION_MAJOR=$(awk '/define VERSION_MAJOR/ { print $3 }' inc/nlarn.h)
FLAGS="-O2 -DG_DISABLE_ASSERT"
LFLAGS=""
if [ -n "$ASAN" ]; then
	FLAGS="-O1 -g -fsanitize=address"   # g_assert() on as in upstream's debug build
	LFLAGS="-sINITIAL_MEMORY=256MB"
else
	LFLAGS="-sINITIAL_MEMORY=64MB"
fi
# -Iport first: port/curses.h, port/panel.h, port/execinfo.h and port/glib
# replace the system headers; game sources stay as they are
emcc $FLAGS -std=gnu99 -Iport -Iport/glib -Iinc -Iinc/external \
	-DG_DISABLE_DEPRECATED \
	-Wall -Wno-unused-parameter -Wno-unused-function \
	src/*.c src/external/*.c port/glib/glib.c port/wcurses.c port/be_web.c \
	-o "$OUT/nlarn-core.js" \
	-sUSE_ZLIB=1 \
	-sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sSTACK_SIZE=4MB \
	-sALLOW_MEMORY_GROWTH $LFLAGS \
	-sEXPORTED_FUNCTIONS=_main \
	-sEXPORTED_RUNTIME_METHODS=FS,IDBFS,ENV,HEAPU32,addRunDependency,removeRunDependency \
	-sFORCE_FILESYSTEM -lidbfs.js -sENVIRONMENT=web \
	--preload-file "$STAGE/lib@/nlarn-data/lib"
cp web/index.html web/nlarn.js "$OUT/"
ls -la "$OUT"
