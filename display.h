/* Written by Richard Christopher, Copyright 2026 Richard Christopher */

#ifndef dis_display_h
#define dis_display_h

#include "header.h"
#include "object.h"

// the window natives, or only 'clock' when dis was built without sdl. the table
// is static storage and outlives every caller
const NativeEntry* displayNatives (int* count);

// idempotent, and a no-op when no window was ever opened
void displayShutdown ();

#endif
