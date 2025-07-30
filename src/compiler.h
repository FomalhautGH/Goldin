#ifndef COMPILER_HEADER 
#define COMPILER_HEADER

#include "lexer.h"

#define NOB_STRIP_PREFIX
#include "nob.h"
#include "stb_ds.h"

typedef struct {
    enum { Byte, Word, DWord, QWord } size;
    enum { Position, Value, Offset } type;
    union { int64_t buffer; size_t position; const char* string; };
} Arg;

typedef enum {
    Add,
    Sub,
    Mul,
    Div,
    Lt,
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
        struct { const char* name; Arg* args; } routine_call;
        struct { Arg offset_dst; Binop op; Arg lhs; Arg rhs; } binop;
        struct { size_t label; Arg arg; } jump_if_not;
        struct { size_t label; } jump;
        struct { size_t index; } label;
    };
} Op;

#define OpAssignLocal(dst, src) (Op) {.type = AssignLocal, .assign_loc = { dst, src }}
#define OpRoutineCall(name, args) (Op) {.type = RoutineCall, .routine_call = { name, args }}
#define OpReserveBytes(bytes) (Op) {.type = ReserveBytes, .reserve_bytes = { bytes }}
#define OpBinary(dst, op, lhs, rhs) (Op) {.type = Binary, .binop = { dst, op, lhs, rhs }}
#define OpJumpIfNot(label, arg) (Op) {.type = JumpIfNot, .jump_if_not = { label, arg }}
#define OpJump(label) (Op) {.type = Jump, .jump = { label }}
#define OpLabel(index) (Op) {.type = Label, .label = { index }}

#define X86_64_LINUX_CALL_REGISTERS_NUM 6

#define max(a, b)               \
   ({ __typeof__ (a) _a = (a);  \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct {
    const char* key;
    Arg value;
} VarsHashmap;

typedef struct {
    Op* ops;
    Arg* data;
    VarsHashmap** local_vars;
    size_t position;
    size_t label_index;
} Compiler;

void init_compiler();
void free_compiler();
bool generate_ops();
Op* get_ops();
Arg* get_data();
const char* display_op(Op op);

#endif
