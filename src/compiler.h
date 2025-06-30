#ifndef COMPILER_HEADER 
#define COMPILER_HEADER

#include <stddef.h>

#define NOB_STRIP_PREFIX
#include "nob.h"
#include "stb_ds.h"

typedef enum {
    QuadWord,
    DoubleWord,
    Word,
    Byte
} VarSize;

typedef enum {
    Var,
    Literal
} ArgType;

typedef union {
    int i32;
    float f32;     // Literal
    size_t offset; // Var
} ArgValue;

typedef struct {
    ArgType type;
    ArgValue value;
} Arg;

typedef enum {
    ReserveBytes,
    LoadVar32,
    StoreVar32,
    RoutineCall 
} OpType;

typedef union {
    size_t num_bytes;                               // ReserveBytes
    size_t load_offset;                             // LoadVar32
    struct { size_t store_offset; int store_arg; }; // StoreVar32
    struct { const char* name; Arg routine_arg; };  // RoutineCall
} OpValue;

typedef struct {
    OpType type;
    OpValue value;
} Op;

typedef struct {
    const char* key;
    int value;
} VarsHashMap;

typedef struct {
} Compiler;

void init_compiler();
void free_compiler();
void dump_ops(Op* ops);
bool generate_ops(Op* ops[]);
bool generate_GAS_x86_64(String_Builder* out, Op* ops);

#endif
