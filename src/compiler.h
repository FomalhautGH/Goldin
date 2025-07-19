#ifndef COMPILER_HEADER 
#define COMPILER_HEADER

#include "lexer.h"
#include <stddef.h>
#include <stdint.h>

#define NOB_STRIP_PREFIX
#include "nob.h"
#include "stb_ds.h"

typedef struct {
    enum { Byte, Word, DWord, QWord, Offset } type;
    union { int64_t value; size_t offset; };
} Arg;

typedef enum {
    Add,
    Sub,
    Mul,
    Div
    // TODO: Pow
    // LeftShift,
    // RightShift,
    // Xor,
    // And,
    // Or,
} Binop;

typedef struct {
    enum {
        ReserveBytes,
        AssignLocal,
        RoutineCall,
        BinaryOperation
    } type;

    union {
        struct { size_t bytes; } reserve_bytes;
        struct { size_t offset_dst; Arg arg; } assign_loc;
        struct { const char* name; Arg arg; } routine_call;
        struct { size_t offset_dst; Binop op; Arg lhs; Arg rhs; } binop;
    };
} Op;

#define OpAssignLocal(dst, src) (Op) {.type = AssignLocal, .assign_loc = { dst, src }}
#define OpRoutineCall(name, arg) (Op) {.type = RoutineCall, .routine_call = { name, arg }}
#define OpReserveBytes(bytes) (Op) {.type = ReserveBytes, .reserve_bytes = { bytes }}

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
