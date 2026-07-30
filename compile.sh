SOURCES="terminal.c memory.c table.c object.c value.c sequence.c numeral.c virtualization.c scanner.c compiler.c debug.c display.c"
CFLAGS=""
LIBS="-lm"

# the window is optional. without sdl2 the build still succeeds and display.c
# contributes only 'clock', so naming a window native is an ordinary undefined
# variable rather than a link error
if [ -z "$DIS_NO_SDL" ] && pkg-config --exists sdl2 2>/dev/null; then
    CFLAGS="-DDIS_DISPLAY $(pkg-config --cflags sdl2)"
    LIBS="$LIBS $(pkg-config --libs sdl2)"
fi

gcc main.c -o dis $SOURCES $CFLAGS $LIBS
