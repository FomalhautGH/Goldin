#ifndef CODEGEN_HEADER
#define CODEGEN_HEADER

#include "compiler.h"

#define NOB_STRIP_PREFIXES
#include "nob.h"

bool generate_GAS_x86_64(String_Builder* out, Op* ops);

#endif
