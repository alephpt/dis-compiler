#include "header.h"
#include "sequence.h"
#include "debug.h"

int main (int argc, const char* argv[]) {
    Sequence seq;
    
    initSequence(&seq);

    int value = addValue(&seq, 9.979);
    writeSequence(&seq, OP_VALUE, 1);
    writeSequence(&seq, value, 1);

    value = addValue(&seq, 332.332);
    writeSequence(&seq, OP_VALUE, 2);
    writeSequence(&seq, value, 2);

    writeSequence(&seq, SIG_RETURN, 3);

    value = addValue(&seq, 0.002);
    writeSequence(&seq, OP_VALUE, 3);
    writeSequence(&seq, value, 3);


    stripSequence(&seq, "Testing Sequence");
    freeSequence(&seq);
    
    return 0;
}
