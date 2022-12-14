#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "header.h"
#include "virtualization.h"
#include "debug.h"
#include "terminal.h"

void repl() {
    char line[1024];

    for (;;) {
        printf("~> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n", line);
            break;
        }
        #ifdef DEBUG_TRACE_EXECUTION    
        printf("\n");// %s", line);
        #endif
        interpret(line);
    }

    return;
}

static char* read_f (const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) { fprintf(stderr, "Error opening \"%s\".", path); exit(74); }
    fseek(f, 0L, SEEK_END);
    size_t fSize = ftell(f);
    rewind(f);

    char* buf = (char*)malloc(fSize + 1);
    if (buf == NULL) { fprintf(stderr, "Out of bounds reading \"%s\".", path); exit(74); }
    size_t bRead = fread(buf, sizeof(char), fSize, f);
    if (bRead < fSize) { fprintf(stderr, "Failed to read \"%s\".", path); exit(74); }
    buf[bRead] = '\0';

    fclose(f);
    return buf;
}

void runFile (const char* file) {
    char* source = read_f(file);
    Interpretation res = interpret(source);
    free(source);
    
    if (res == COMPILE_ERROR) exit(65);
    if (res == RUNTIME_ERROR) exit(70);

    return;
}