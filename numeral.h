#ifndef dis_numeral_h
#define dis_numeral_h

#include "header.h"
#include "value.h"

#define NUMERAL_TYPE(value)     (AS_NUMERAL(value)->type)
#define IS_HEXADECIMAL(value)   isNumeralType(value, N_HEXADECIMAL)
#define IS_DENARY(value)        isNumeralType(value, N_DENARY)
#define IS_OCTAL(value)         isNumeralType(value, N_OCTAL)
#define IS_BINARY(value)        isNumeralType(value, N_BINARY)

typedef enum {
    N_BINARY,
    N_OCTAL,
    N_DENARY,
    N_HEXADECIMAL
} NumeralT;

struct Numeral {
    NumeralT type;
};

struct NHexadecimal {
    Numeral numeral;
    char* number;
};

struct NDenary {
    Numeral numeral;
    double number;
};

struct NOctal {
    Numeral numeral;
    int number;
}

struct NBinary {
    Numeral numeral;
    int number;
}

static inline bool isNumeralType(Value value, NumeralT type) {
    return IS_NUMERAL(value) && AS_NUMERAL(value)->type == type;
}

#endif