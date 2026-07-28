// Written by Richard Christopher, Copyright 2026 Richard Christopher

#include <stdlib.h>
#include "numeral.h"

static int digitOf (char c) {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

// the scanner hands over the digits with the '0b'/'0c'/'0x' prefix stripped
static double accumulate (const char* chars, int len, int base) {
    double total = 0;

    for (int i = 0; i < len; i++) {
        int digit = digitOf(chars[i]);

        if (digit < 0 || digit >= base) { break; }

        total = (total * base) + digit;
    }

    return total;
}

double parseNumeral (const char* chars, int len, NumeralT base) {
    if (base == N_DENARY) { return strtod(chars, NULL); }

    return accumulate(chars, len, (int)base);
}
