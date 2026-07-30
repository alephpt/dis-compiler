/* Written by Richard Christopher, Copyright 2026 Richard Christopher */

// The window. Everything sdl touches lives behind DIS_DISPLAY, which compile.sh
// only defines when pkg-config finds sdl2. Without it this file still builds and
// still offers 'clock' - the window natives are simply never registered, so
// naming one is an ordinary undefined variable rather than a link failure.

#include <time.h>
#include "display.h"
#include "virtualization.h"

// a monotonic millisecond count. no sdl anywhere in it, so it is available in
// every build and is the clock a frame loop should time itself against
static bool nativeClock (int args, Value* argv, Value* result) {
    struct timespec now;

    (void)args;
    (void)argv;

    clock_gettime(CLOCK_MONOTONIC, &now);

    *result = NUMERAL_VALUE(((double)now.tv_sec * 1000.0) + ((double)now.tv_nsec / 1000000.0));
    return true;
}


#ifdef DIS_DISPLAY

#include <SDL.h>

// one window, one renderer, one streaming texture. a singleton rather than a
// handle object, so there is no lifetime for a dis program to get wrong
typedef struct {
    bool open;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int w;
    int h;
    int textureW;
    int textureH;
    Uint32 textureFormat;
    int wheelY;
    bool quit;
    bool clicked[8];
    bool pressed[SDL_NUM_SCANCODES];
} Display;

static Display display;

static bool displayOpen (const char* what) {
    if (!display.open) {
        runtimeErr("%s needs a display to be open.", what);
        return false;
    }

    return true;
}

static bool displayCoord (Value value, const char* what, int* out) {
    if (!IS_NUMERAL(value)) {
        runtimeErr("%s must be a numeral.", what);
        return false;
    }

    double v = AS_NUMERAL(value);

    // the range settles in doubles - narrowing one that does not fit would be
    // undefined, and the result would pass every check after it
    if (!(v >= -2147483648.0) || !(v <= 2147483647.0) || v != (double)(int64_t)v) {
        runtimeErr("%s must be a whole number.", what);
        return false;
    }

    *out = (int)v;
    return true;
}

static bool scancodeOf (Value value, const char* what, SDL_Scancode* out) {
    if (!IS_STRING(value)) {
        runtimeErr("%s takes the name of a key.", what);
        return false;
    }

    SDL_Scancode code = SDL_GetScancodeFromName(AS_CSTRING(value));

    if (code == SDL_SCANCODE_UNKNOWN) {
        runtimeErr("'%s' is not the name of a key.", AS_CSTRING(value));
        return false;
    }

    *out = code;
    return true;
}

// 'display -> w, h, "title".' - sdl is started here and nowhere else, so a
// program that never opens a window never touches the video subsystem at all
static bool nativeDisplay (int args, Value* argv, Value* result) {
    int w, h;

    (void)args;

    if (display.open) {
        runtimeErr("A display is already open.");
        return false;
    }

    if (!displayCoord(argv[0], "The display width", &w)) { return false; }
    if (!displayCoord(argv[1], "The display height", &h)) { return false; }

    if (!IS_STRING(argv[2])) {
        runtimeErr("The display title must be a string.");
        return false;
    }

    if (w <= 0 || h <= 0) {
        runtimeErr("A display must be at least one pixel across.");
        return false;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        runtimeErr("%s", SDL_GetError());
        return false;
    }

    display.window = SDL_CreateWindow(AS_CSTRING(argv[2]),
                                      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      w, h, SDL_WINDOW_SHOWN);

    if (display.window == NULL) {
        runtimeErr("%s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    display.renderer = SDL_CreateRenderer(display.window, -1, SDL_RENDERER_ACCELERATED);

    // a machine with no accelerated path still gets a window
    if (display.renderer == NULL) {
        display.renderer = SDL_CreateRenderer(display.window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (display.renderer == NULL) {
        runtimeErr("%s", SDL_GetError());
        SDL_DestroyWindow(display.window);
        display.window = NULL;
        SDL_Quit();
        return false;
    }

    display.w = w;
    display.h = h;
    display.open = true;

    *result = NUMERAL_VALUE(1);
    return true;
}

// 'blit -> fb, w, h.' - one to one with the window, no scaling and no stretch
static bool nativeBlit (int args, Value* argv, Value* result) {
    int w, h;

    (void)args;

    if (!displayOpen("blit")) { return false; }

    if (!IS_BUFFER(argv[0])) {
        runtimeErr("blit requires a buffer.");
        return false;
    }

    OBuffer* frame = AS_BUFFER(argv[0]);
    int stride = frame->form->stride;

    if (!displayCoord(argv[1], "The blit width", &w)) { return false; }
    if (!displayCoord(argv[2], "The blit height", &h)) { return false; }

    if (stride != 4 && stride != 3) {
        runtimeErr("blit requires three or four byte pixels, but '%s' is %d.",
                   frame->form->name->chars, stride);
        return false;
    }

    if ((double)w * (double)h != (double)frame->count) {
        runtimeErr("blit was given %d by %d pixels but '%s' holds %d.",
                   w, h, frame->form->name->chars, frame->count);
        return false;
    }

    if (w != display.w || h != display.h) {
        runtimeErr("blit is %d by %d but the display is %d by %d.",
                   w, h, display.w, display.h);
        return false;
    }

    Uint32 format = (stride == 4) ? SDL_PIXELFORMAT_RGBA32 : SDL_PIXELFORMAT_RGB24;

    // the texture is only rebuilt when its shape actually changes
    if (display.texture == NULL || display.textureW != w ||
        display.textureH != h || display.textureFormat != format) {
        if (display.texture != NULL) { SDL_DestroyTexture(display.texture); }

        display.texture = SDL_CreateTexture(display.renderer, format,
                                            SDL_TEXTUREACCESS_STREAMING, w, h);

        if (display.texture == NULL) {
            runtimeErr("%s", SDL_GetError());
            return false;
        }

        display.textureW = w;
        display.textureH = h;
        display.textureFormat = format;
    }

    if (SDL_UpdateTexture(display.texture, NULL, frame->bytes, w * stride) != 0) {
        runtimeErr("%s", SDL_GetError());
        return false;
    }

    if (SDL_RenderCopy(display.renderer, display.texture, NULL, NULL) != 0) {
        runtimeErr("%s", SDL_GetError());
        return false;
    }

    *result = NUMERAL_VALUE(frame->count);
    return true;
}

static bool nativePresent (int args, Value* argv, Value* result) {
    (void)args;
    (void)argv;

    if (!displayOpen("present")) { return false; }

    SDL_RenderPresent(display.renderer);

    *result = NONE_VALUE;
    return true;
}

// every edge lasts exactly one pump - clicked, pressed and the wheel all reset
// here and then record whatever this drain turns up
static bool nativePump (int args, Value* argv, Value* result) {
    SDL_Event event;
    int seen = 0;

    (void)args;
    (void)argv;

    if (!displayOpen("pump")) { return false; }

    memset(display.clicked, 0, sizeof(display.clicked));
    memset(display.pressed, 0, sizeof(display.pressed));
    display.wheelY = 0;

    while (SDL_PollEvent(&event)) {
        seen++;

        switch (event.type) {
            case SDL_QUIT:
                display.quit = true;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) { display.quit = true; }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button < 8) { display.clicked[event.button.button] = true; }
                break;
            case SDL_MOUSEWHEEL:
                display.wheelY += event.wheel.y;
                break;
            case SDL_KEYDOWN:
                if (!event.key.repeat) { display.pressed[event.key.keysym.scancode] = true; }
                break;
            default:
                break;
        }
    }

    *result = NUMERAL_VALUE(seen);
    return true;
}

static bool nativeQuitting (int args, Value* argv, Value* result) {
    (void)args;
    (void)argv;

    if (!displayOpen("quitting")) { return false; }

    *result = BOOLEAN_VALUE(display.quit);
    return true;
}

static bool nativeMouseX (int args, Value* argv, Value* result) {
    int x, y;

    (void)args;
    (void)argv;

    if (!displayOpen("mousex")) { return false; }

    SDL_GetMouseState(&x, &y);

    *result = NUMERAL_VALUE(x);
    return true;
}

static bool nativeMouseY (int args, Value* argv, Value* result) {
    int x, y;

    (void)args;
    (void)argv;

    if (!displayOpen("mousey")) { return false; }

    SDL_GetMouseState(&x, &y);

    *result = NUMERAL_VALUE(y);
    return true;
}

static bool nativeButton (int args, Value* argv, Value* result) {
    int which;

    (void)args;

    if (!displayOpen("button")) { return false; }
    if (!displayCoord(argv[0], "The button number", &which)) { return false; }

    if (which < 1 || which > 7) {
        *result = BOOLEAN_VALUE(false);
        return true;
    }

    Uint32 held = SDL_GetMouseState(NULL, NULL);

    *result = BOOLEAN_VALUE((held & SDL_BUTTON((Uint32)which)) != 0);
    return true;
}

static bool nativeClicked (int args, Value* argv, Value* result) {
    int which;

    (void)args;

    if (!displayOpen("clicked")) { return false; }
    if (!displayCoord(argv[0], "The button number", &which)) { return false; }

    if (which < 0 || which > 7) {
        *result = BOOLEAN_VALUE(false);
        return true;
    }

    *result = BOOLEAN_VALUE(display.clicked[which]);
    return true;
}

static bool nativeWheel (int args, Value* argv, Value* result) {
    (void)args;
    (void)argv;

    if (!displayOpen("wheel")) { return false; }

    *result = NUMERAL_VALUE(display.wheelY);
    return true;
}

static bool nativeKey (int args, Value* argv, Value* result) {
    SDL_Scancode code;

    (void)args;

    if (!displayOpen("key")) { return false; }
    if (!scancodeOf(argv[0], "key", &code)) { return false; }

    const Uint8* held = SDL_GetKeyboardState(NULL);

    *result = BOOLEAN_VALUE(held[code] != 0);
    return true;
}

static bool nativePressed (int args, Value* argv, Value* result) {
    SDL_Scancode code;

    (void)args;

    if (!displayOpen("pressed")) { return false; }
    if (!scancodeOf(argv[0], "pressed", &code)) { return false; }

    *result = BOOLEAN_VALUE(display.pressed[code]);
    return true;
}

static bool nativeShut (int args, Value* argv, Value* result) {
    (void)args;
    (void)argv;

    displayShutdown();

    *result = NONE_VALUE;
    return true;
}

void displayShutdown () {
    if (!display.open) { return; }

    if (display.texture != NULL) { SDL_DestroyTexture(display.texture); }
    if (display.renderer != NULL) { SDL_DestroyRenderer(display.renderer); }
    if (display.window != NULL) { SDL_DestroyWindow(display.window); }

    SDL_Quit();

    memset(&display, 0, sizeof(display));
    return;
}

#else

// nothing was ever opened, so there is nothing to close
void displayShutdown () { return; }

#endif


static const NativeEntry displayTable[] = {
#ifdef DIS_DISPLAY
    { "display",  nativeDisplay,  3 },
    { "blit",     nativeBlit,     3 },
    { "present",  nativePresent,  0 },
    { "pump",     nativePump,     0 },
    { "quitting", nativeQuitting, 0 },
    { "mousex",   nativeMouseX,   0 },
    { "mousey",   nativeMouseY,   0 },
    { "button",   nativeButton,   1 },
    { "clicked",  nativeClicked,  1 },
    { "wheel",    nativeWheel,    0 },
    { "key",      nativeKey,      1 },
    { "pressed",  nativePressed,  1 },
    { "shut",     nativeShut,     0 },
#endif
    { "clock",    nativeClock,    0 },
};

const NativeEntry* displayNatives (int* count) {
    *count = (int)(sizeof(displayTable) / sizeof(NativeEntry));
    return displayTable;
}
