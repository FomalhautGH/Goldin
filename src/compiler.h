#ifndef COMPILER_HEADER 
#define COMPILER_HEADER

#include "lexer.h"
#include <stddef.h>

#define NOB_STRIP_PREFIX
#include "nob.h"
#include "stb_ds.h"

typedef enum {
    Byte, 
    Word, 
    DWord, 
    QWord 
} Size;

typedef struct {
    Size size;
    bool is_signed;
    enum { 
        Position,
        Value,
        Offset,
        ReturnVal
    } type;
    union { 
        int64_t buffer;
        size_t position;
        char* string;
    };
} Arg;

typedef enum {
    Add,
    Sub,
    Mul,
    Div,
    Lt,
    Gt,
    Le,
    Ge,
    Eq,
    Ne,
    RSh,
    LSh
} BinaryOp;

typedef enum {
    Not,
    Deref,
    Ref
} UnaryOp;

typedef struct {
    enum {
        NewRoutine,
        RtReturn,
        AssignLocal,
        RoutineCall,
        Binary,
        Unary,
        Label,
        JumpIfNot,
        Jump
    } type;

    union {
        struct { char* name; size_t bytes; Arg* args; } new_routine;
        struct { Arg ret; } return_routine;
        struct { Arg offset_dst; Arg arg; } assign_loc;
        struct { char* name; Arg* args; } routine_call;
        struct { Arg offset_dst; BinaryOp op; Arg lhs; Arg rhs; } binop;
        struct { Arg offset_dst; UnaryOp op; Arg arg; } unary;
        struct { size_t label; Arg arg; } jump_if_not;
        struct { size_t label; } jump;
        struct { size_t index; } label;
    };
} Op;

#define OpAssignLocal(dst, src) (Op) {.type = AssignLocal, .assign_loc = { dst, src }}
#define OpRoutineCall(name, args) (Op) {.type = RoutineCall, .routine_call = { name, args }}
#define OpBinary(dst, op, lhs, rhs) (Op) {.type = Binary, .binop = { dst, op, lhs, rhs }}
#define OpUnary(dst, op, arg) (Op) {.type = Unary, .unary = { dst, op, arg }}
#define OpJumpIfNot(label, arg) (Op) {.type = JumpIfNot, .jump_if_not = { label, arg }}
#define OpJump(label) (Op) {.type = Jump, .jump = { label }}
#define OpLabel(index) (Op) {.type = Label, .label = { index }}
#define OpNewRoutine(name, bytes, args) (Op) { .type = NewRoutine, .new_routine = { name, bytes, args }}
#define OpReturn(arg) (Op) { .type = RtReturn, .return_routine.ret = arg }

#define X86_64_LINUX_CALL_REGISTERS_NUM 6

#define max(a, b)               \
   ({ __typeof__ (a) _a = (a);  \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct {
    char* key;
    Arg value;
} VarsHashmap;

typedef struct {
    Op* ops;
    Arg* static_data;
    VarsHashmap** local_vars;
    size_t position;
    size_t label_index;
    bool returned;
} Compiler;

void init_compiler();
void free_compiler();
bool generate_ops();
Op* get_ops();
Arg* get_data();
const char* display_op(Op op);

#endif
