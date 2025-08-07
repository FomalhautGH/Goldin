#include "codegen.h"
#include "compiler.h"
#include "token.h"
#include <assert.h>

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

static const char* x86_64_linux_rcx_registers[] = {
    "cl", "cx", "ecx", "rcx"
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

static void append_immediate(String_Builder* out, Arg arg) {
    switch (arg.size) {
        case Byte: sb_appendf(out, "%d", get_byte(arg)); break;
        case Word: sb_appendf(out, "%d", get_word(arg)); break;
        case DWord: sb_appendf(out, "%d", get_dword(arg)); break;
        case QWord: sb_appendf(out, "%ld", get_qword(arg)); break;
        default: UNREACHABLE("Invalid Arg size");
    }
}

static void append_ptr_dimension(String_Builder* out, Size size) {
    switch (size) {
        case Byte: sb_appendf(out, "byte"); break;
        case Word: sb_appendf(out, "word"); break;
        case DWord: sb_appendf(out, "dword"); break;
        case QWord: sb_appendf(out, "qword"); break;
        default: UNREACHABLE("Invalid Arg size");
    }
}

static const char* right_mov(Destination dst, Arg src, const char** reg_arg) {
    if (dst.size <= src.size) return "mov";

    if (src.is_signed) {
        if (src.size == DWord) return "movsxd";
        return "movsx";
    } 

    if (src.size == DWord) {
        *reg_arg = x86_64_linux_rbx_registers[src.size];
        return "mov";
    } else {
        return "movzx";
    }
}

static void mov_to_register(String_Builder* out, Destination dst, Arg arg) {
    const char* dst_reg = dst.value.reg;

    switch (arg.type) {
        case Position: {
            const char* reg_arg = x86_64_linux_rbx_registers[dst.size];
            const char* mov_instr = right_mov(dst, arg, &reg_arg);

            sb_appendf(out, "    %s %s, ", mov_instr, reg_arg);
            if (dst.size <= arg.size) append_ptr_dimension(out, dst.size);
            else append_ptr_dimension(out, arg.size);
            sb_appendf(out, " ptr [rbp - %zu]\n", arg.position);
        } break;
        case Offset: {
            assert(arg.size == QWord && dst.size == QWord);
            sb_appendf(out, "    movabs %s, offset .str_%zu\n", dst_reg, arg.position);
        } break;
        case ReturnVal: {
            sb_appendf(out, "    mov %s, %s\n", dst_reg, x86_64_linux_rax_registers[dst.size]);
        } break;
        case Value: {
            sb_appendf(out, "    mov %s, ", dst_reg);
            append_immediate(out, arg);
            sb_appendf(out, "\n");
        } break;
        default: UNREACHABLE("Invalid Arg type");
    }
}

static void mov_to_memory(String_Builder* out, Destination dst, Arg arg) {
    size_t dst_pos = dst.value.position;

    switch (arg.type) {
        case Position: {
            const char* reg_dst = x86_64_linux_rbx_registers[dst.size];
            const char* reg_arg = reg_dst;
            const char* mov_instr = right_mov(dst, arg, &reg_arg);

            sb_appendf(out, "    %s %s, ", mov_instr, reg_arg);
            if (dst.size <= arg.size) append_ptr_dimension(out, dst.size);
            else append_ptr_dimension(out, arg.size);
            sb_appendf(out, " ptr [rbp - %zu]\n", arg.position);

            sb_appendf(out, "    mov ");
            append_ptr_dimension(out, dst.size);
            sb_appendf(out, " ptr [rbp - %zu], ", dst_pos);
            sb_appendf(out, "%s\n", reg_dst);
        } break;
        case Offset: {
            sb_appendf(out, "    movabs rax, offset .str_%zu\n", arg.position);
            sb_appendf(out, "    mov qword ptr [rbp - %zu], rax\n", dst_pos);
        } break;
        case ReturnVal: {
            const char* reg_arg = x86_64_linux_rax_registers[dst.size];
            sb_appendf(out, "    mov ");
            append_ptr_dimension(out, dst.size);
            sb_appendf(out, " ptr [rbp - %zu], %s\n", dst_pos, reg_arg);
        } break;
        case Value: {
            sb_appendf(out, "    mov ");
            append_ptr_dimension(out, dst.size);
            sb_appendf(out, " ptr [rbp - %zu], ", dst_pos);
            append_immediate(out, arg);
            sb_appendf(out, "\n");
        } break;
        default: UNREACHABLE("Invalid Arg type");
    }
}

static void mov(String_Builder* out, Destination dst, Arg arg) {
    switch (dst.type) {
        case Register: mov_to_register(out, dst, arg); break;
        case Memory: mov_to_memory(out, dst, arg); break;
        default: UNREACHABLE("Invalide Destination type");
    }
}

static void routine_call_arg_value(String_Builder* out, Arg arg, size_t reg_index) {
    const char* reg = x86_64_linux_call_registers[reg_index][arg.size];
    sb_appendf(out, "    mov %s, ", reg);
    append_immediate(out, arg);
    sb_appendf(out, "\n");
}

static void routine_call_arg_position(String_Builder* out, Arg arg, size_t reg_index) {
    const char* reg = x86_64_linux_call_registers[reg_index][arg.size];
    sb_appendf(out, "    mov %s, ", reg);
    append_ptr_dimension(out, arg.size);
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
            append_immediate(out, arg);
        } break;
        case Position: {
            append_ptr_dimension(out, arg.size);
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
    append_ptr_dimension(out, dst.size);
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
    Arg arg_dst = op.binop.offset_dst;

    Destination dst = {
        .type = Register,
        .size = arg_dst.size,
        .value.reg = x86_64_linux_rbx_registers[arg_dst.size]
    };

    mov(out, dst, lhs);

    switch (rhs.type) {
        case Position: {
            const char* rbx_reg = x86_64_linux_rbx_registers[rhs.size];
            sb_appendf(out, "    imul %s, [rbp - %zu]\n", rbx_reg, rhs.position);
            sb_appendf(out, "    mov [rbp - %zu], %s\n", arg_dst.position, rbx_reg);
        } break;
        case ReturnVal: {
            const char* rbx_reg = x86_64_linux_rbx_registers[lhs.size];
            const char* rax_reg = x86_64_linux_rax_registers[lhs.size];
            sb_appendf(out, "    imul %s, %s\n", rbx_reg, rax_reg);
            sb_appendf(out, "    mov [rbp - %zu], %s\n", arg_dst.position, rbx_reg);
        } break;
        case Value: { 
            const char* rbx_reg = x86_64_linux_rbx_registers[lhs.size];
            sb_appendf(out, "    imul %s, %s, ", rbx_reg, rbx_reg);
            append_immediate(out, rhs);
            sb_appendf(out, "\n    mov [rbp - %zu], %s\n", arg_dst.position, rbx_reg);
        } break;
        case Offset: UNREACHABLE("Unsupported"); break;
        default: UNREACHABLE("Invalid Arg type");
    }
}

static void binary_operation_cmp(String_Builder* out, Op op, const char* instr) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg dst = op.binop.offset_dst;

    // TODO: support typecheking in expressions in order to always have the same size
    // assert(lhs.size == rhs.size);

    binary_operation_load_factor(out, lhs, "mov");
    binary_operation_load_factor(out, rhs, "cmp");
    binary_operation_load_cmp_dst(out, dst, instr);
}

static void binary_operation_shift(String_Builder* out, Op op) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg arg_dst = op.binop.offset_dst;

    const char* instr = NULL;

    switch (op.binop.op) {
        case LSh: instr = "sal"; break;
        case RSh: instr = "sar"; break;
        default: UNREACHABLE("");
    }

    Destination dst = {
        .type = Register,
        .size = arg_dst.size,
        .value.reg = x86_64_linux_rbx_registers[arg_dst.size]
    };

    mov(out, dst, lhs);

    switch (rhs.type) {
        case Position: { TODO("");
        } break;
        case ReturnVal: { TODO("");
        } break;
        case Value: {
            const char* rbx_reg = x86_64_linux_rbx_registers[arg_dst.size];
            sb_appendf(out, "    %s %s, ", instr, rbx_reg);
            append_immediate(out, rhs);
            sb_appendf(out, "\n    mov [rbp - %zu], %s\n", arg_dst.position, rbx_reg);
        } break;
        case Offset: UNREACHABLE("Unsupported"); break;
        default: UNREACHABLE("Invalid Arg type");
    }
}

static void binary_operation(String_Builder* out, Op op) {
    BinaryOp operation = op.binop.op;

    switch (operation) {
        case Add: binary_operation_add(out, op); break;
        case Sub: binary_operation_sub(out, op); break;
        // TODO: these instructions are all signed 
        // https://cs.brown.edu/courses/cs033/docs/guides/x64_cheatsheet.pdf
        case Mul: binary_operation_mul(out, op); break;
        case Eq: binary_operation_cmp(out, op, "sete"); break;
        case Lt: binary_operation_cmp(out, op, "setl"); break;
        case Le: binary_operation_cmp(out, op, "setle"); break;
        case Gt: binary_operation_cmp(out, op, "setg"); break;
        case Ge: binary_operation_cmp(out, op, "setge"); break;
        case Ne: binary_operation_cmp(out, op, "setne"); break;
        case LSh:
        case RSh: binary_operation_shift(out, op); break;
        default: TODO("Binary operation unsupported yet");
    } 
}

static void jump_if_not(String_Builder* out, Op op) {
    Arg arg = op.jump_if_not.arg;

    const char* reg = x86_64_linux_rax_registers[arg.size];
    sb_appendf(out, "    mov %s, ", reg);

    switch (arg.type) {
        case Value: append_immediate(out, arg); break;
        case Position: {
            append_ptr_dimension(out, arg.size);
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

static void assign_local_value(String_Builder* out, Arg arg_dst, Arg arg) {
    assert(arg_dst.type == Position);

    Destination dst = {
        .type = Memory,
        .size = arg_dst.size,
        .value.position = arg_dst.position
    };

    mov(out, dst, arg);
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
                append_immediate(out, return_value);
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

static void unary(String_Builder* out, Op op) {
    UnaryOp unop = op.unary.op;
    Arg arg = op.unary.arg;
    Arg dst = op.unary.offset_dst;

    assert(arg.type == Position);

    switch (unop) {
        case Deref: { 
            sb_appendf(out, "    mov rbx, qword ptr [rbp - %zu]\n", arg.position);
            sb_appendf(out, "    mov rbx, qword ptr [rbx]\n");
            sb_appendf(out, "    mov [rbp - %zu], rbx\n", dst.position);
        } break;
        case Ref: {
            sb_appendf(out, "    lea rbx, [rbp - %zu]\n", arg.position);
            sb_appendf(out, "    mov [rbp - %zu], rbx\n", dst.position);
        }; break;
        case Not: TODO(""); break;
        default: UNREACHABLE("Invalid Unary Operation");
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
            case Unary: unary(out, op); break;
            default: UNREACHABLE("Unsupported Operation");
        }
        // sb_appendf(out, "\n");
    }

    static_data(out, data);
    return true;
}
