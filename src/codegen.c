#include "codegen.h"
#include "compiler.h"
#include "token.h"

#define DIMENTIONS 4

static const char* x86_64_linux_call_registers[X86_64_LINUX_CALL_REGISTERS_NUM][DIMENTIONS] = {
    {"dil", "di", "edi", "rdi"},
    {"sil", "si", "esi", "rsi"},
    {"dl",  "dx", "edx", "rdx"},
    {"cl",  "cx", "ecx", "rcx"},
    {"r8b", "r8w", "r8d", "r8"},
    {"r9b", "r9w", "r9d", "r9"}
};

static const char* x86_64_linux_temp_registers[] = {
    "al", "ax", "eax", "rax"
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

static void binary_operation_load_factor(String_Builder* out, Arg arg, const char* ins) {
    sb_appendf(out, "    %s %s, ", ins, x86_64_linux_temp_registers[arg.size]);

    switch (arg.type) {
        case Value: insert_immediate(out, arg); break;
        case Position: {
           insert_ptr_dimension(out, arg);
           sb_appendf(out, " ptr [rbp - %zu]", arg.position);
        } break;
        default: UNREACHABLE("Invalid Arg type");
    }

    sb_appendf(out, "\n");
}

static void binary_operation_load_dst(String_Builder* out, Arg dst) {
    const char* reg = x86_64_linux_temp_registers[dst.size];
    if (dst.type != Position) UNREACHABLE("Destination Arg can only be of the position type");

    sb_appendf(out, "    mov ");
    insert_ptr_dimension(out, dst);
    sb_appendf(out, " ptr [rbp - %zu], %s\n", dst.position, reg);
}

static void binary_operation_load_cmp_dst(String_Builder* out, Arg dst) {
    if (dst.type != Position) UNREACHABLE("Destination Arg can only be of the position type");
    if (dst.size != Byte) UNREACHABLE("Destination Arg con only be of size byte");
    sb_appendf(out, "    setl al\n");
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

    // TODO: support typecheking in expressions in order to always have the same size
    assert(lhs.size == rhs.size);
    assert(lhs.size == dst.size);

    binary_operation_load_factor(out, lhs, "mov");
    binary_operation_load_factor(out, rhs, "sub");
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
    binary_operation_load_factor(out, lhs, "mov");
    binary_operation_load_factor(out, rhs, "imul");
    binary_operation_load_dst(out, dst);
}

static void binary_operation_lessthan(String_Builder* out, Op op) {
    Arg lhs = op.binop.lhs;
    Arg rhs = op.binop.rhs;
    Arg dst = op.binop.offset_dst;

    // TODO: support typecheking in expressions in order to always have the same size
    assert(lhs.size == rhs.size);

    binary_operation_load_factor(out, lhs, "mov");
    binary_operation_load_factor(out, rhs, "cmp");
    binary_operation_load_cmp_dst(out, dst);
}

static void binary_operation(String_Builder* out, Op op) {
    Binop operation = op.binop.op;

    switch (operation) {
        case Add: binary_operation_add(out, op); break;
        case Sub: binary_operation_sub(out, op); break;
        case Mul: binary_operation_mul(out, op); break;
        case Lt: binary_operation_lessthan(out, op); break;
        default: TODO("Binary operation unsupported yet");
    } 
}

static void jump_if_not(String_Builder* out, Op op) {
    Arg arg = op.jump_if_not.arg;

    const char* reg = x86_64_linux_temp_registers[arg.size];
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
            const char* reg_arg = x86_64_linux_temp_registers[arg.size];
            const char* reg_dst = x86_64_linux_temp_registers[dst.size];
            sb_appendf(out, "    mov %s, ", reg_arg);
            insert_ptr_dimension(out, arg);
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
    size_t bytes = op.reserve_bytes.bytes;
    if (bytes > 0) sb_appendf(out, "    sub rsp, %zu\n", bytes);
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

    size_t len = arrlenu(ops);
    for (size_t i = 0; i < len; ++i) {
        Op op = ops[i];
        sb_appendf(out, "# %s\n", display_op(op));
        switch (op.type) {
            case RoutineCall: routine_call(out, op); break;
            case AssignLocal: assign_local(out, op); break;
            case ReserveBytes: reserve_bytes(out, op); break;
            case Binary: binary_operation(out, op); break;
            case JumpIfNot: jump_if_not(out, op); break;
            case Jump: jump(out, op); break;
            case Label: label(out, op); break;
        }
        sb_appendf(out, "\n");
    }

    function_epilog(out);
    static_data(out, data);
    return true;
}
