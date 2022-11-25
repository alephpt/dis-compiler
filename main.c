#include "header.h"
#include "sequence.h"
#include "debug.h"

int main (int argc, const char* argv[]) {
    Sequence seq;
    
    initSequence(&seq);

    int value = addValue(&seq, 9.979);
    writeSequence(&seq, OP_VALUE);
    writeSequence(&seq, value);

    value = addValue(&seq, 332.332);
    writeSequence(&seq, OP_VALUE);
    writeSequence(&seq, value);

    writeSequence(&seq, SIG_RETURN);
    stripSequence(&seq, "Testing Sequence");
    freeSequence(&seq);
    
    return 0;
}
