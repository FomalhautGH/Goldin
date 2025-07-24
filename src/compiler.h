#ifndef COMPILER_HEADER 
#define COMPILER_HEADER

#include "lexer.h"
#include <stddef.h>
#include <stdint.h>

#define NOB_STRIP_PREFIX
#include "nob.h"
#include "stb_ds.h"

typedef struct {
    enum { Byte, Word, DWord, QWord } size;
    enum { Offset, Value } type;
    union { int64_t buffer; size_t offset; };
} Arg;

typedef enum {
    Add,
    Sub,
    Mul,
    Div
} Binop;

typedef struct {
    enum {
        ReserveBytes,
        AssignLocal,
        RoutineCall,
        Binary
    } type;

    union {
        struct { size_t bytes; } reserve_bytes;
        struct { Arg offset_dst; Arg arg; } assign_loc;
        struct { const char* name; Arg arg; } routine_call;
        struct { Arg offset_dst; Binop op; Arg lhs; Arg rhs; } binop;
    };
} Op;

#define OpAssignLocal(dst, src) (Op) {.type = AssignLocal, .assign_loc = { dst, src }}
#define OpRoutineCall(name, arg) (Op) {.type = RoutineCall, .routine_call = { name, arg }}
#define OpReserveBytes(bytes) (Op) {.type = ReserveBytes, .reserve_bytes = { bytes }}
#define OpBinary(dst, op, lhs, rhs) (Op) {.type = Binary, .binop = { dst, op, lhs, rhs }}

typedef struct {
    const char* key;
    int value;
} VarsHashMap;

typedef struct {
    Op* ops;
    VarsHashMap* vars;
    size_t offset;
} Compiler;

void init_compiler();
void free_compiler();
bool generate_ops();
Op* get_ops();

#endif
