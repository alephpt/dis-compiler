#include <stdio.h>
#include "header.h"
#include "compiler.h"
#include "scanner.h"

void compile(const char* source, Sequence* sequence) {
    int line = -1;
    initScanner(source);
    advance();
    express()
    consume(T_EOF, "Expected End of Expression.");
}