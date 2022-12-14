#include <stdio.h>
#include "virtualization.h"
#include "terminal.h"

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