#ifndef dis_debug_h
#define dis_debug_h

#include "sequence.h"

void stripSequence (Sequence* sequence, const char* name);
int stripCommand (Sequence* sequence, int offset);

#endif