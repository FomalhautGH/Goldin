#include "codegen.h"
#include "compiler.h"
#include "token.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define DIMENTIONS 4

static const char* x86_64_linux_call_registers[X86_64_LINUX_CALL_REGISTERS_NUM][DIMENTIONS] = {
    {"dil", "di", "edi", "rdi"},
    {"sil", "si", "esi", "rsi"},
    {"dl",  "dx", "edx", "rdx"},
    {"cl",  "cx", "ecx", "rcx"},
    {"r8b", "r8w", "r8d", "r8"},
    {"r9b", "r9w", "r9d", "r9"}
};

static const char* x86_64_linux_rax_registers[] = {
    "al", "ax", "eax", "rax"
};

static const char* x86_64_linux_rbx_registers[] = {
    "bl", "bx", "ebx", "rbx"
};

static size_t round_to_next_pow2(size_t value) {
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return ++value;
}

static int8_t get_byte(Arg arg) {
    int8_t result = 0;
    memcpy(&result, &arg.buffer, sizeof(int8_t));
    return result;
}

static int16_t get_word(Arg arg) {
    int16_t result = 0;
    memcpy(&result, &arg.buffer, sizeof(int16_t));
    return result;
}

static int32_t get_dword(Arg arg) {
    int32_t result = 0;
    memcpy(&result, &arg.buffer, sizeof(int32_t));
    return result;
}

static int64_t get_qword(Arg arg) {
    int64_t result = 0;
    memcpy(&result, &arg.buffer, sizeof(int64_t));
    return result;
}

static void insert_immediate(String_Builder* out, Arg arg) {
    switch (arg.size) {
        case Byte: sb_appendf(out, "%d", get_byte(arg)); break;
        case Word: sb_appendf(out, "%d", get_word(arg)); break;
        case DWord: sb_appendf(out, "%d", get_dword(arg)); break;
        case QWord: sb_appendf(out, "%ld", get_qword(arg)); break;
        default: UNREACHABLE("Invalid Arg size");
    }
}

static void insert_ptr_dimension(String_Builder* out, Arg arg) {
    switch (arg.size) {
        case Byte: sb_appendf(out, "byte"); break;
        case Word: sb_appendf(out, "word"); break;
        case DWord: sb_appendf(out, "dword"); break;
        case QWord: sb_appendf(out, "qword"); break;
        default: UNREACHABLE("Invalid Arg size");
    }
}

static void routine_call_arg_value(String_Builder* out, Arg arg, size_t reg_index) {
    const char* reg = x86_64_linux_call_registers[reg_index][arg.size];
    sb_appendf(out, "    mov %s, ", reg);
    insert_immediate(out, arg);
    sb_appendf(out, "\n");
}

static void routine_call_arg_position(String_Builder* out, Arg arg, size_t reg_index) {
    const char* reg = x86_64_linux_call_registers[reg_index][arg.size];
    sb_appendf(out, "    mov %s, ", reg);
    insert_ptr_dimension(out, arg);
    sb_appendf(out, " ptr [rbp - %zu]\n", arg.position);
}

static void routine_call_arg_offset(String_Builder* out, Arg arg, size_t reg_index) {
    assert(arg.size == QWord);
    sb_appendf(out, "    movabs %s, offset .str_%zu\n", x86_64_linux_call_registers[reg_index][QWord], arg.position);
}

static void routine_call_arg_returnval(String_Builder* out, Arg arg, size_t reg_index) {
    const char* reg = x86_64_linux_call_registers[reg_index][arg.size];
    const char* return_reg = x86_64_linux_rax_registers[arg.size];
    sb_appendf(out, "    mov %s, %s\n", reg, return_reg);
}

static void routine_call(String_Builder* out, Op op) {
    Arg* args = op.routine_call.args;

    for (size_t i = 0; i < arrlenu(args); ++i) {
        Arg arg = args[i];
        switch (arg.type) {
            case Value: routine_call_arg_value(out, arg, i); break;
            case Position: routine_call_arg_position(out, arg, i); break;
            case Offset: routine_call_arg_offset(out, arg, i); break;
            case ReturnVal: routine_call_arg_returnval(out, arg, i); break;
            default: UNREACHABLE("Invalid Arg type");
        }
    }

    // TODO: Support variadics
    if (strcmp(op.routine_call.name, "printf") == 0) sb_appendf(out, "    mov al, 0\n"); 
    sb_appendf(out, "    call %s\n", op.routine_call.name);
}

static void binary_operation_load_factor(String_Builder* out, Arg arg, const char* ins) {
    sb_appendf(out, "    %s %s, ", ins, x86_64_linux_rbx_registers[arg.size]);

    switch (arg.type) {
        case Value: {
            insert_immediate(out, arg);
        } break;
        case Position: {
            insert_ptr_dimension(out, arg);
            sb_appendf(out, " ptr [rbp - %zu]", arg.position);
        } break;
        case ReturnVal: sb_appendf(out, "%s", x86_64_linux_rax_registers[arg.size]); break;
        default: UNREACHABLE("Invalid Arg type");
    }

    sb_appendf(out, "\n");
}

static void binary_operation_load_dst(String_Builder* out, Arg dst) {
    const char* reg = x86_64_linux_rbx_registers[dst.size];
    if (dst.type != Position) UNREACHABLE("Destination Arg can only be of the position type");

    sb_appendf(out, "    mov ");
    insert_ptr_dimension(out, dst);
    sb_appendf(out, " ptr [rbp - %zu], %s\n", dst.position, reg);
}

static void binary_operation_load_cmp_dst(String_Builder* out, Arg dst, const char* instr) {
    if (dst.type != Position) UNREACHABLE("Destination Arg can only be of the position type");
    if (dst.size != Byte) UNREACHABLE("Destination Arg con only be of size byte");
    sb_appendf(out, "    %s al\n", instr);
    sb_appendf(out, "    mov byte ptr [rbp - %zu], al\n", dst.position);
}

static void binary_operation_add(String_Builder* out, Op op) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg dst = op.binop.offset_dst;

    binary_operation_load_factor(out, lhs, "mov");
    binary_operation_load_factor(out, rhs, "add");
    binary_operation_load_dst(out, dst);
}

static void binary_operation_sub(String_Builder* out, Op op) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg dst = op.binop.offset_dst;

    binary_operation_load_factor(out, lhs, "mov");
    binary_operation_load_factor(out, rhs, "sub");
    binary_operation_load_dst(out, dst);
}

static void binary_operation_mul(String_Builder* out, Op op) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg dst = op.binop.offset_dst;

    // TODO: Use imul with 3 arguments since it exists in x64
    assert(lhs.type == Position && rhs.type == ReturnVal);

    const char* reg = x86_64_linux_rbx_registers[lhs.size];
    sb_appendf(out, "    mov %s, ", reg);
    insert_ptr_dimension(out, lhs);
    sb_appendf(out, " ptr [rbp - %zu]\n", lhs.position);

    sb_appendf(out, "    imul %s, %s\n", reg, x86_64_linux_rax_registers[rhs.size]);
    sb_appendf(out, "    mov [rbp - %zu], %s\n", dst.position, reg);
}

static void binary_operation_cmp(String_Builder* out, Op op, const char* instr) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg dst = op.binop.offset_dst;

    // TODO: support typecheking in expressions in order to always have the same size
    assert(lhs.size == rhs.size);

    binary_operation_load_factor(out, lhs, "mov");
    binary_operation_load_factor(out, rhs, "cmp");
    binary_operation_load_cmp_dst(out, dst, instr);
}

static void binary_operation(String_Builder* out, Op op) {
    Binop operation = op.binop.op;

    switch (operation) {
        case Add: binary_operation_add(out, op); break;
        case Sub: binary_operation_sub(out, op); break;
        case Mul: binary_operation_mul(out, op); break;
        // TODO: these instructions are all signed 
        // https://cs.brown.edu/courses/cs033/docs/guides/x64_cheatsheet.pdf
        case Eq: binary_operation_cmp(out, op, "sete"); break;
        case Lt: binary_operation_cmp(out, op, "setl"); break;
        case Le: binary_operation_cmp(out, op, "setle"); break;
        case Gt: binary_operation_cmp(out, op, "setg"); break;
        case Ge: binary_operation_cmp(out, op, "setge"); break;
        case Ne: binary_operation_cmp(out, op, "setne"); break;
        default: TODO("Binary operation unsupported yet");
    } 
}

static void jump_if_not(String_Builder* out, Op op) {
    Arg arg = op.jump_if_not.arg;

    const char* reg = x86_64_linux_rax_registers[arg.size];
    sb_appendf(out, "    mov %s, ", reg);

    switch (arg.type) {
        case Value: insert_immediate(out, arg); break;
        case Position: {
            insert_ptr_dimension(out, arg);
            sb_appendf(out, " ptr [rbp - %zu]\n", arg.position);
        } break;
        default: UNREACHABLE("Invalid Arg type");
    }

    sb_appendf(out, "    test %s, %s\n", reg, reg);
    sb_appendf(out, "    jz .in_%zu\n", op.jump_if_not.label);
}

static void jump(String_Builder* out, Op op) {
    sb_appendf(out, "    jmp .in_%zu\n", op.jump.label);
}

static void label(String_Builder* out, Op op) {
    sb_appendf(out, ".in_%zu:\n", op.label.index);
}

static void assign_local_value(String_Builder* out, Arg dst, Arg arg) {
    switch (arg.type) {
        case Value: {
            sb_appendf(out, "    mov ");
            insert_ptr_dimension(out, dst);
            sb_appendf(out, " ptr [rbp - %zu], ", dst.position);
            insert_immediate(out, arg);
            sb_appendf(out, "\n");
        } break;
        case Position: {
            const char* instr = NULL;
            const char* reg_dst = x86_64_linux_rax_registers[dst.size];
            const char* reg_arg = reg_dst;

            if (dst.size <= arg.size) {
                instr = "mov";
            } else if (arg.is_signed) {
                if (arg.size == DWord) {
                    instr = "movsxd";
                } else {
                    instr = "movsx";
                }
            } else {
                if (arg.size == DWord) {
                    reg_arg = x86_64_linux_rax_registers[arg.size];
                    instr = "mov";
                } else {
                    instr = "movzx";
                }
            }

            sb_appendf(out, "    %s %s, ", instr, reg_arg);
            if (dst.size <= arg.size) insert_ptr_dimension(out, dst);
            else insert_ptr_dimension(out, arg);
            sb_appendf(out, " ptr [rbp - %zu]\n", arg.position);

            sb_appendf(out, "    mov ");
            insert_ptr_dimension(out, dst);
            sb_appendf(out, " ptr [rbp - %zu], ", dst.position);
            sb_appendf(out, "%s\n", reg_dst);
        } break;
        case Offset: {
            sb_appendf(out, "    movabs rax, offset .str_%zu\n", arg.position);
            sb_appendf(out, "    mov qword ptr [rbp - %zu], rax\n", dst.position);
        } break;
        case ReturnVal: {
            const char* reg_arg = x86_64_linux_rax_registers[arg.size];
            sb_appendf(out, "    mov ");
            insert_ptr_dimension(out, dst);
            sb_appendf(out, " ptr [rbp - %zu], %s\n", dst.position, reg_arg);
        } break;
        default: UNREACHABLE("Invalid Arg type");
    }
}

static void assign_local(String_Builder* out, Op op) {
    Arg src = op.assign_loc.arg;
    Arg dst = op.assign_loc.offset_dst;

    switch (dst.type) {
        case Position: assign_local_value(out, dst, src); break;
        default: UNREACHABLE("Destination Arg can only be of the offset type");
    }
}

static void routine_prolog(String_Builder* out, Op op) {
    sb_appendf(out, ".globl %s\n", op.new_routine.name);
    sb_appendf(out, "%s:\n", op.new_routine.name);
    sb_appendf(out, "    push rbp\n");
    sb_appendf(out, "    mov rbp, rsp\n");

    size_t bytes = op.new_routine.bytes;
    if (bytes > 0) sb_appendf(out, "    sub rsp, %ld\n", round_to_next_pow2(bytes));

    for (size_t i = 0; i < arrlenu(op.new_routine.args); ++i) {
        Arg arg = op.new_routine.args[i];
        sb_appendf(out, "    mov [rbp - %zu], %s\n", arg.position, x86_64_linux_call_registers[i][arg.size]);
    }
}

static void routine_epilog(String_Builder* out, Op op) {
    Arg return_value = op.return_routine.ret;
    if (return_value.position == 0) sb_appendf(out, "    xor rax, rax\n");
    else {
        const char* reg = x86_64_linux_rax_registers[return_value.size];
        switch (return_value.type) {
            // TODO: just strings for now
            case Offset: sb_appendf(out, "    movabs rax, offset .str_%zu\n", return_value.position); break;
            case Position: sb_appendf(out, "    mov %s, [rbp - %zu]\n", reg, return_value.position); break;
            case Value: {
                sb_appendf(out, "    mov %s, ", reg); 
                insert_immediate(out, return_value);
                sb_appendf(out, "\n");
            } break;
            default: UNREACHABLE("Invalid Arg type");
        }
    }

    sb_appendf(out, "    mov rsp, rbp\n");
    sb_appendf(out, "    pop rbp\n");
    sb_appendf(out, "    ret\n");
}

static void generate_static_data(String_Builder* out, Arg arg, size_t index) {
    assert(arg.type == Offset);
    sb_appendf(out, ".str_%zu:\n", index);
    sb_appendf(out, "    .asciz \"%s\"\n", arg.string);
    sb_appendf(out, "    .size .str_%zu, %zu\n", index, strlen(arg.string) + 1);
}

static void static_data(String_Builder* out, Arg* data) {
    for (size_t i = 0; i < arrlenu(data); ++i) {
        generate_static_data(out, data[i], i);
    }
}

bool generate_GAS_x86_64(String_Builder* out, Op* ops, Arg* data) {
    sb_appendf(out, ".intel_syntax noprefix\n");
    sb_appendf(out, ".text\n");

    size_t len = arrlenu(ops);
    for (size_t i = 0; i < len; ++i) {
        Op op = ops[i];
        // sb_appendf(out, "# %s\n", display_op(op));
        switch (op.type) {
            case RoutineCall: routine_call(out, op); break;
            case NewRoutine: routine_prolog(out, op); break;
            case RtReturn: routine_epilog(out, op); break;
            case AssignLocal: assign_local(out, op); break;
            case Binary: binary_operation(out, op); break;
            case JumpIfNot: jump_if_not(out, op); break;
            case Jump: jump(out, op); break;
            case Label: label(out, op); break;
        }
        // sb_appendf(out, "\n");
    }

    static_data(out, data);
    return true;
}
