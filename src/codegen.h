#ifndef CODEGEN_HEADER
#define CODEGEN_HEADER

#include "compiler.h"

#define NOB_STRIP_PREFIXES
#include "nob.h"

typedef struct {
    Size size;
    enum { Register, Memory } type;
    union { const char* reg; size_t position; } value;
} Destination;

#define NewDestination(size, reg) (Destination) { .size = size, .type = Register, .dst.reg = reg }

bool generate_GAS_x86_64(String_Builder* out, Op* ops, Arg* data);

#endif
