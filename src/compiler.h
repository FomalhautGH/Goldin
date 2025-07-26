#ifndef COMPILER_HEADER 
#define COMPILER_HEADER

#include "lexer.h"

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
    Div,
    LessThan,
} Binop;

typedef struct {
    enum {
        ReserveBytes,
        AssignLocal,
        RoutineCall,
        Binary,
        Label,
        JumpIfNot,
        Jump
    } type;

    union {
        struct { size_t bytes; } reserve_bytes;
        struct { Arg offset_dst; Arg arg; } assign_loc;
        struct { const char* name; Arg arg; } routine_call;
        struct { Arg offset_dst; Binop op; Arg lhs; Arg rhs; } binop;
        struct { size_t label; Arg arg; } jump_if_not;
        struct { size_t label; } jump;
        struct { size_t index; } label;
    };
} Op;

#define OpAssignLocal(dst, src) (Op) {.type = AssignLocal, .assign_loc = { dst, src }}
#define OpRoutineCall(name, arg) (Op) {.type = RoutineCall, .routine_call = { name, arg }}
#define OpReserveBytes(bytes) (Op) {.type = ReserveBytes, .reserve_bytes = { bytes }}
#define OpBinary(dst, op, lhs, rhs) (Op) {.type = Binary, .binop = { dst, op, lhs, rhs }}
#define OpJumpIfNot(label, arg) (Op) {.type = JumpIfNot, .jump_if_not = { label, arg }}
#define OpJump(label) (Op) {.type = Jump, .jump = { label }}
#define OpLabel(index) (Op) {.type = Label, .label = { index }}

typedef struct {
    const char* key;
    int value;
} VarsHashmap;

typedef struct {
    Op* ops;
    VarsHashmap** local_vars;
    size_t offset;
    size_t label_index;
} Compiler;

void init_compiler();
void free_compiler();
bool generate_ops();
Op* get_ops();

#endif
