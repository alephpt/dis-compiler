#include "header.h"
#include "virt.h"
#include "sequence.h"
#include "debug.h"

int main (int argc, const char* argv[]) {
    initVM();
    Sequence seq;
    
    initSequence(&seq);

    int value = addValue(&seq, 0.02);
    writeSequence(&seq, OP_VALUE, 1);
    writeSequence(&seq, value, 1);

    value = addValue(&seq, 332.332);
    writeSequence(&seq, OP_VALUE, 1);
    writeSequence(&seq, value, 1);

    writeSequence(&seq, SIG_NEG, 1);

    writeSequence(&seq, SIG_MULT, 2);

    value = addValue(&seq, 0.02002);
    writeSequence(&seq, OP_VALUE, 3);
    writeSequence(&seq, value, 3);
    
    writeSequence(&seq, SIG_DIV, 3);


    writeSequence(&seq, SIG_RETURN, 1);


//    stripSequence(&seq, "Debug Main");

    interpret(&seq);



    freeVM();
    freeSequence(&seq);
    
    return 0;
}