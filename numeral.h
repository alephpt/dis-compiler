#ifndef dis_numeral_h
#define dis_numeral_h

#include "header.h"

// the enum value is the radix of the literal it tags
typedef enum {
    N_BINARY = 2,           // 0b1010
    N_OCTAL = 8,            // 0c17
    N_DENARY = 10,          // 255 || 1.5
    N_HEXADECIMAL = 16      // 0xFF
} NumeralT;

// every literal resolves into the single numeral representation of the vm
double parseNumeral (const char* chars, int len, NumeralT base);

#endif
