#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "header.h"
#include "virt.h"
#include "sequence.h"
#include "debug.h"

static void repl() {
    char line[1024];

    for (;;) {
        printf("~> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n", line);
            break;
        }

        printf("<~ %s", line);
        //interpret(line);
    }

    return;
}

static char* readFile (const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) { fprintf(stderr, "Error opening \"%s\".", path); exit(74); }
    fseek(file, 0L, SEEK_END);
    size_t fSize = ftell(file);
    rewind(file);

    char* buf = (char*)malloc(fSize + 1);
    if (buf == NULL) { fprintf(stderr, "Out of bounds reading \"%s\".", path); exit(74); }
    size_t bRead = fread(buf, sizeof(char), fSize, f);
    if (bRead < fSize) { fprintf(stderr, "Failed to read \"%s\".", path); exit(74); }
    buf[bRead] = '\0';

    fclose(f);
    return buf;
}

static void runFile (const char* file) {
    char* source = readFile(file);
    Interpretation res = interpret(source);
    free(source);
    
    if (res == COMPILER_ERROR) exit(65);
    if (res == RUNTIME_ERROR) exit(70);

    return;
}

int main (int argc, const char* argv[]) {
    initVM();
    
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: disC [path]\n");
    }

    freeVM();
    return 0;
}