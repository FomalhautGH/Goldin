#include "codegen.h"
#include "compiler.h"
#include "token.h"
#include <assert.h>

static const char* x86_64_linux_call_registers[6][4] = {
    {"dil", "di", "edi", "rdi"},
    {"sil", "si", "esi", "rsi"},
    {"dl",  "dx", "edx", "rdx"},
    {"cl",  "cx", "ecx", "rcx"},
    {"r8b", "r8w", "r8d", "r8"},
    {"r9b", "r9w", "r9d", "r9"}
};

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

static void routine_call_arg_value(String_Builder* out, Arg arg, size_t reg_index) {
    switch (arg.size) {
        case Byte: sb_appendf(out, "    mov %s, %d\n", x86_64_linux_call_registers[reg_index][Byte], get_byte(arg)); break;
        case Word: sb_appendf(out, "    mov %s, %d\n", x86_64_linux_call_registers[reg_index][Word], get_word(arg)); break;
        case DWord: sb_appendf(out, "    mov %s, %d\n", x86_64_linux_call_registers[reg_index][DWord], get_dword(arg)); break;
        case QWord: sb_appendf(out, "    mov %s, %ld\n", x86_64_linux_call_registers[reg_index][QWord], get_qword(arg)); break;
        default: UNREACHABLE("Invalid Arg size");
    }
}

static void routine_call_arg_position(String_Builder* out, Arg arg, size_t reg_index) {
    switch (arg.size) {
        case Byte: sb_appendf(out, "    mov %s, byte ptr [rbp - %zu]\n", x86_64_linux_call_registers[reg_index][Byte], arg.position); break;
        case Word: sb_appendf(out, "    mov %s, word ptr [rbp - %zu]\n", x86_64_linux_call_registers[reg_index][Word], arg.position); break;
        case DWord: sb_appendf(out, "    mov %s, dword ptr [rbp - %zu]\n", x86_64_linux_call_registers[reg_index][DWord], arg.position); break;
        case QWord: sb_appendf(out, "    mov %s, qword ptr [rbp - %zu]\n", x86_64_linux_call_registers[reg_index][QWord], arg.position); break;
        default: UNREACHABLE("Invalid Arg size");
    }
}

static void routine_call_arg_offset(String_Builder* out, Arg arg, size_t reg_index) {
    assert(arg.size == QWord);
    sb_appendf(out, "    mov %s, offset .str_%zu\n", x86_64_linux_call_registers[reg_index][QWord], arg.position);
}

static void routine_call(String_Builder* out, Op op) {
    Arg* args = op.routine_call.args;

    for (size_t i = 0; i < arrlenu(args); ++i) {
        Arg arg = args[i];
        switch (arg.type) {
            case Value: routine_call_arg_value(out, arg, i); break;
            case Position: routine_call_arg_position(out, arg, i); break;
            case Offset: routine_call_arg_offset(out, arg, i); break;
            default: UNREACHABLE("Invalid Arg type");
        }
    }

    sb_appendf(out, "    mov al, 0\n"); // TODO: Support variadics
    sb_appendf(out, "    call %s\n", op.routine_call.name);
}

static void binary_operation_load_lhs_factor(String_Builder* out, Arg lhs) {
    switch (lhs.type) {
        case Value:
            switch (lhs.size) {
                case Byte: sb_appendf(out, "    mov al, %d\n", get_byte(lhs)); break;
                case Word: sb_appendf(out, "    mov ax, %d\n", get_word(lhs)); break;
                case DWord: sb_appendf(out, "    mov eax, %d\n", get_dword(lhs)); break;
                case QWord: sb_appendf(out, "    mov rax, %ld\n", get_qword(lhs)); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        case Position:
            switch (lhs.size) {
                case Byte: sb_appendf(out, "    mov al, byte ptr [rbp - %zu]\n", lhs.position); break;
                case Word: sb_appendf(out, "    mov ax, word ptr [rbp - %zu]\n", lhs.position); break;
                case DWord: sb_appendf(out, "    mov eax, dword ptr [rbp - %zu]\n", lhs.position); break;
                case QWord: sb_appendf(out, "    mov rax, qword ptr [rbp - %zu]\n", lhs.position); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        default: UNREACHABLE("Invalid Arg type");
    }
}

static void binary_operation_load_add_rhs_factor(String_Builder* out, Arg rhs) {
    switch (rhs.type) {
        case Value:
            switch (rhs.size) {
                case Byte: sb_appendf(out, "    add al, %d\n", get_byte(rhs)); break;
                case Word: sb_appendf(out, "    add ax, %d\n", get_word(rhs)); break;
                case DWord: sb_appendf(out, "    add eax, %d\n", get_dword(rhs)); break;
                case QWord: sb_appendf(out, "    add rax, %ld\n", get_qword(rhs)); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        case Position:
            switch (rhs.size) {
                case Byte: sb_appendf(out, "    add al, byte ptr [rbp - %zu]\n", rhs.position); break;
                case Word: sb_appendf(out, "    add ax, word ptr [rbp - %zu]\n", rhs.position); break;
                case DWord: sb_appendf(out, "    add eax, dword ptr [rbp - %zu]\n", rhs.position); break;
                case QWord: sb_appendf(out, "    add rax, qword ptr [rbp - %zu]\n", rhs.position); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        default: UNREACHABLE("Invalid Arg type");
    }
}

static void binary_operation_load_sub_rhs_factor(String_Builder* out, Arg rhs) {
    switch (rhs.type) {
        case Value:
            switch (rhs.size) {
                case Byte: sb_appendf(out, "    sub al, %d\n", get_byte(rhs)); break;
                case Word: sb_appendf(out, "    sub ax, %d\n", get_word(rhs)); break;
                case DWord: sb_appendf(out, "    sub eax, %d\n", get_dword(rhs)); break;
                case QWord: sb_appendf(out, "    sub rax, %ld\n", get_qword(rhs)); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        case Position:
            switch (rhs.size) {
                case Byte: sb_appendf(out, "    sub al, byte ptr [rbp - %zu]\n", rhs.position); break;
                case Word: sb_appendf(out, "    sub ax, word ptr [rbp - %zu]\n", rhs.position); break;
                case DWord: sb_appendf(out, "    sub eax, dword ptr [rbp - %zu]\n", rhs.position); break;
                case QWord: sb_appendf(out, "    sub rax, qword ptr [rbp - %zu]\n", rhs.position); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        default: UNREACHABLE("Invalid Arg type");
    }
}

static void binary_operation_load_cmp_rhs_factor(String_Builder* out, Arg rhs) {
    switch (rhs.type) {
        case Value:
            switch (rhs.size) {
                case Byte: sb_appendf(out, "    cmp al, %d\n", get_byte(rhs)); break;
                case Word: sb_appendf(out, "    cmp ax, %d\n", get_word(rhs)); break;
                case DWord: sb_appendf(out, "    cmp eax, %d\n", get_dword(rhs)); break;
                case QWord: sb_appendf(out, "    cmp rax, %ld\n", get_qword(rhs)); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        case Position:
            switch (rhs.size) {
                case Byte: sb_appendf(out, "    cmp al, byte ptr [rbp - %zu]\n", rhs.position); break;
                case Word: sb_appendf(out, "    cmp ax, word ptr [rbp - %zu]\n", rhs.position); break;
                case DWord: sb_appendf(out, "    cmp eax, dword ptr [rbp - %zu]\n", rhs.position); break;
                case QWord: sb_appendf(out, "    cmp rax, qword ptr [rbp - %zu]\n", rhs.position); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        default: UNREACHABLE("Invalid Arg type");
    }
}

static void binary_operation_load_mul_rhs_factor(String_Builder* out, Arg rhs) {
    switch (rhs.type) {
        case Value:
            switch (rhs.size) {
                case Byte: sb_appendf(out, "    imul al, %d\n", get_byte(rhs)); break;
                case Word: sb_appendf(out, "    imul ax, %d\n", get_word(rhs)); break;
                case DWord: sb_appendf(out, "    imul eax, %d\n", get_dword(rhs)); break;
                case QWord: sb_appendf(out, "    imul rax, %ld\n", get_qword(rhs)); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        case Position:
            switch (rhs.size) {
                case Byte: sb_appendf(out, "    imul al, byte ptr [rbp - %zu]\n", rhs.position); break;
                case Word: sb_appendf(out, "    imul ax, word ptr [rbp - %zu]\n", rhs.position); break;
                case DWord: sb_appendf(out, "    imul eax, dword ptr [rbp - %zu]\n", rhs.position); break;
                case QWord: sb_appendf(out, "    imul rax, qword ptr [rbp - %zu]\n", rhs.position); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        default: UNREACHABLE("Invalid Arg type");
    }
}

static void binary_operation_load_dst(String_Builder* out, Arg dst) {
    switch (dst.type) {
        case Position:
            switch (dst.size) {
                case Byte: sb_appendf(out, "    mov byte ptr [rbp - %zu], al\n", dst.position); break;
                case Word: sb_appendf(out, "    mov word ptr [rbp - %zu], ax\n", dst.position); break;
                case DWord: sb_appendf(out, "    mov dword ptr [rbp - %zu], eax\n", dst.position); break;
                case QWord: sb_appendf(out, "    mov qword ptr [rbp - %zu], rax\n", dst.position); break;
            } break;
        default: UNREACHABLE("Destination Arg can only be of the offset type");
    }
}

static void binary_operation_load_cmp_dst(String_Builder* out, Arg dst) {
    switch (dst.type) {
        case Position:
            switch (dst.size) {
                case Byte: sb_appendf(out, "    setl al\n");
                           sb_appendf(out, "    mov byte ptr [rbp - %zu], al\n", dst.position); break;
                default: UNREACHABLE("Destination Arg can only be of size byte");
            } break;
        default: UNREACHABLE("Destination Arg can only be of the offset type");
    }
}

static void binary_operation_add(String_Builder* out, Op op) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg dst = op.binop.offset_dst;

    // TODO: support typecheking in expressions in order to always have the same size
    assert(lhs.size == rhs.size);
    assert(lhs.size == dst.size);

    binary_operation_load_lhs_factor(out, lhs);
    binary_operation_load_add_rhs_factor(out, rhs);
    binary_operation_load_dst(out, dst);
}

static void binary_operation_sub(String_Builder* out, Op op) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg dst = op.binop.offset_dst;

    // TODO: support typecheking in expressions in order to always have the same size
    assert(lhs.size == rhs.size);
    assert(lhs.size == dst.size);

    binary_operation_load_lhs_factor(out, lhs);
    binary_operation_load_sub_rhs_factor(out, rhs);
    binary_operation_load_dst(out, dst);
}

static void binary_operation_mul(String_Builder* out, Op op) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg dst = op.binop.offset_dst;

    // TODO: support typecheking in expressions in order to always have the same size
    assert(lhs.size == rhs.size);
    assert(lhs.size == dst.size);

    // TODO: Use imul with 3 arguments since it exists in x64
    binary_operation_load_lhs_factor(out, lhs);
    binary_operation_load_mul_rhs_factor(out, rhs);
    binary_operation_load_dst(out, dst);
}

static void binary_operation_lessthan(String_Builder* out, Op op) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg dst = op.binop.offset_dst;

    // TODO: support typecheking in expressions in order to always have the same size
    assert(lhs.size == rhs.size);

    // TODO: Use imul with 3 arguments since it exists in x64
    binary_operation_load_lhs_factor(out, lhs);
    binary_operation_load_cmp_rhs_factor(out, rhs);
    binary_operation_load_cmp_dst(out, dst);
}

static void binary_operation(String_Builder* out, Op op) {
    Binop operation = op.binop.op;

    switch (operation) {
        case Add: binary_operation_add(out, op); break;
        case Sub: binary_operation_sub(out, op); break;
        case Mul: binary_operation_mul(out, op); break;
        case LessThan: binary_operation_lessthan(out, op); break;
        default: TODO("Binary operation unsupported yet");
    } 
}

static void jump_if_not(String_Builder* out, Op op) {
    Arg arg = op.jump_if_not.arg;

    switch (arg.type) {
        case Value:
            switch (arg.size) {
                case Byte: sb_appendf(out, "    mov al, %d\n", get_byte(arg)); break;
                case Word: sb_appendf(out, "    mov ax, %d\n", get_word(arg)); break;
                case DWord: sb_appendf(out, "    mov eax, %d\n", get_dword(arg)); break;
                case QWord: sb_appendf(out, "    mov rax, %ld\n", get_qword(arg)); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        case Position:
            switch (arg.size) {
                case Byte: sb_appendf(out, "    mov al, byte ptr [rbp - %zu]\n", arg.position); break;
                case Word: sb_appendf(out, "    mov ax, word ptr [rbp - %zu]\n", arg.position); break;
                case DWord: sb_appendf(out, "    mov eax, dword ptr [rbp - %zu]\n", arg.position); break;
                case QWord: sb_appendf(out, "    mov rax, qword ptr [rbp - %zu]\n", arg.position); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        default: UNREACHABLE("Invalid Arg type");
    }

    switch (arg.size) {
        case Byte: sb_appendf(out, "    test al, al\n"); break;
        case Word: sb_appendf(out, "    test ax, ax\n"); break;
        case DWord: sb_appendf(out, "    test eax, eax\n"); break;
        case QWord: sb_appendf(out, "    test rax, rax\n"); break;
        default: UNREACHABLE("Invalid Arg size");
    }

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
        case Value:
            switch (arg.size) {
                case Byte: 
                    sb_appendf(out, "    mov byte ptr [rbp - %zu], %d\n", dst.position, get_byte(arg)); break;
                case Word: 
                    sb_appendf(out, "    mov word ptr [rbp - %zu], %d\n", dst.position, get_word(arg)); break;
                case DWord: 
                    sb_appendf(out, "    mov dword ptr [rbp - %zu], %d\n", dst.position, get_dword(arg)); break;
                case QWord: 
                    sb_appendf(out, "    mov qword ptr [rbp - %zu], %ld\n", dst.position, get_qword(arg)); break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        case Position:
            switch (arg.size) {
                case Byte:
                    sb_appendf(out, "    mov al, byte ptr [rbp - %zu]\n", arg.position);
                    sb_appendf(out, "    mov byte ptr [rbp - %zu], al\n", dst.position);
                    break;
                case Word:
                    sb_appendf(out, "    mov ax, word ptr [rbp - %zu]\n", arg.position);
                    sb_appendf(out, "    mov word ptr [rbp - %zu], ax\n", dst.position);
                    break;
                case DWord:
                    sb_appendf(out, "    mov eax, dword ptr [rbp - %zu]\n", arg.position);
                    sb_appendf(out, "    mov dword ptr [rbp - %zu], eax\n", dst.position);
                    break;
                case QWord:
                    sb_appendf(out, "    mov rax, qword ptr [rbp - %zu]\n", arg.position);
                    sb_appendf(out, "    mov qword ptr [rbp - %zu], rax\n", dst.position);
                    break;
                default: UNREACHABLE("Invalid Arg size");
            } break;
        case Offset: 
            sb_appendf(out, "    movabs rax, offset .str_%zu\n", arg.position);
            sb_appendf(out, "    mov qword ptr [rbp - %zu], rax\n", dst.position);
            break;
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

static void reserve_bytes(String_Builder* out, Op op) {
    if (op.reserve_bytes.bytes > 0)
        sb_appendf(out, "    sub rsp, %zu\n", op.reserve_bytes.bytes);
}

static void function_prolog(String_Builder* out, const char* name) {
    sb_appendf(out, ".globl %s\n", name);
    sb_appendf(out, "%s:\n", name);
    sb_appendf(out, "    push rbp\n");
    sb_appendf(out, "    mov rbp, rsp\n");
}

static void function_epilog(String_Builder* out) {
    sb_appendf(out, "    mov rsp, rbp\n");
    sb_appendf(out, "    xor rax, rax\n");
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
    function_prolog(out, "main");

    size_t len = arrlen(ops);
    for (size_t i = 0; i < len; ++i) {
        Op op = ops[i];
        switch (op.type) {
            case RoutineCall: routine_call(out, op); break;
            case AssignLocal: assign_local(out, op); break;
            case ReserveBytes: reserve_bytes(out, op); break;
            case Binary: binary_operation(out, op); break;
            case JumpIfNot: jump_if_not(out, op); break;
            case Jump: jump(out, op); break;
            case Label: label(out, op); break;
        }
    }

    function_epilog(out);
    static_data(out, data);
    return true;
}
