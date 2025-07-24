#include "compiler.h"
#include "lexer.h"
#include "token.h"

// static Compiler comp = {0};
void init_compiler() {}
void free_compiler() {}

static void offset_arg(Arg* arg_ptr, int size, size_t offset) {
    arg_ptr->size = size;
    arg_ptr->type = Offset;
    arg_ptr->offset = offset;
}

static bool consume() {
    return next_token();
}

static bool expect_type(TokenType type) {
    if (get_token().type != type) {
        error_expected(type, get_token().type);
        return false;
    }
    return true;
}

static bool consume_and_expect(TokenType type) {
    next_token();
    if (!expect_type(type)) return false; 
    return true;
}

static bool compile_primary_expression(Op* ops[], Arg* arg, VarsHashMap* vars) {
    // TODO: Check errno in convertions and overflow if literal too big
    // TODO: Pratt parser

    switch (get_type()) {
        case Identifier: {
            const char* name = get_value().items;
            size_t offset = shget(vars, name);
            offset_arg(arg, DWord, offset);
            consume();
        } break;
        case IntLiteral: {
            int32_t value = strtol(get_value().items, NULL, 10);
            arg->size = DWord;
            arg->type = Value;
            memcpy(&arg->buffer, &value, sizeof(int32_t));
            consume();
        } break;
        case LeftParen: TODO("Groupings unsupported yet"); break;
        case RealLiteral: TODO("Floats unsupported yet"); break;
        case StringLiteral: TODO("String usopported yet"); break;
        default: UNREACHABLE("Unsupported token type in primary expression");
    }

    return true;
}

// TODO: change tokentype to a type that represents binding powers more effectively
static bool compile_expression_wrapped(Op* ops[], Arg* arg, VarsHashMap* vars, TokenType min_binding, size_t* offset) {
    compile_primary_expression(ops, arg, vars);

    while (get_type() > min_binding) {
        switch (get_type()) {
            case Plus: {
                consume();
                Arg rhs = {0};
                compile_expression_wrapped(ops, &rhs, vars, Plus, offset);

                // TODO: offset needs to change based off the type of the expression not simply 4
                *offset += 4;

                Arg dst = {0};
                offset_arg(&dst, DWord, *offset);
                arrput(*ops, OpBinary(dst, Add, *arg, rhs));
                offset_arg(arg, DWord, *offset);
            } break;
            case Minus: TODO("Unsupported minus op"); break;
            case Slash: TODO("Unsupported div op"); break;
            case Star: TODO("Unsupported mul op"); break;
            default: goto exit;
            // default: printf("%s\n", display_type(get_type())); UNREACHABLE("Unsupported token type in expression");
        }
    }

exit:
    return true;
}

static bool compile_expression(Op* ops[], Arg* arg, VarsHashMap* vars, size_t* offset) {
    return compile_expression_wrapped(ops, arg, vars, 0, offset);
}

static bool declare_var(Op* ops[], TokenType var_type, size_t* offset, VarsHashMap* vars) {
    if (!consume_and_expect(Identifier)) return false;
    const char* var_name = strdup(get_value().items);

    if (shget(vars, var_name) != -1) {
        error_msg("COMPILATION ERROR: Redefinition of variable");
        return false;
    }

    size_t bytes = 0;
    switch (var_type) {
        case VarTypeu8:
        case VarTypei8: bytes = 1; break;

        case VarTypeu16:
        case VarTypei16: bytes = 2; break;
        case VarTypef32: TODO("Float not supported yet");
        case VarTypeu32:
        case VarTypei32: bytes = 4; break;

        case VarTypef64: TODO("Double not supported yet");
        case VarTypeu64:
        case VarTypei64: bytes = 8; break;

        default: UNREACHABLE("Not a valid var type");
    }

    *offset += bytes;
    shput(vars, var_name, *offset);

    consume();
    if (get_type() == Equal) {
        consume();

        Arg arg = {0};
        if (!compile_expression(ops, &arg, vars, offset)) return false;

        Arg dst = {0};
        offset_arg(&dst, DWord, *offset);
        arrput(*ops, OpAssignLocal(dst, arg));
    }

    if (!expect_type(SemiColon)) return false;
    return true;
}

static bool routine(Op* ops[], VarsHashMap* vars, size_t* offset) {
    const char* name = strdup(get_value().items);
    if (!consume_and_expect(LeftParen)) return false;
    consume();

    Arg arg = {0};
    if (!compile_expression(ops, &arg, vars, offset)) return false;
    arrput(*ops, OpRoutineCall(name, arg));

    if (!expect_type(RightParen)) return false;
    if (!consume_and_expect(SemiColon)) return false;

    return true;
}

static bool compile_routine_body(Op* ops[]) {
    size_t offset = 0;
    // TODO: Can this pointer change in functions?
    VarsHashMap* vars = NULL;
    shdefault(vars, -1);

    consume();
    if (strcmp(get_value().items, "main") != 0) {
        error_msg("COMPILATION ERROR: Only main function supported right now");
        return false;
    }

    // TODO: function arguments
    if (!consume_and_expect(LeftParen)) return false;
    if (!consume_and_expect(RightParen)) return false;
    if (!consume_and_expect(LeftBracket)) return false;

    while (true) {
        consume();
        switch (get_type()) {
            case VarTypei8:
            case VarTypei16:
            case VarTypei32:
            case VarTypei64:
            case VarTypeu8:
            case VarTypeu16:
            case VarTypeu32:
            case VarTypeu64:
            case VarTypef32:
            case VarTypef64: if (!declare_var(ops, get_type(), &offset, vars)) return false; break;

             // TODO: Only routines call for now and no arity checked
            case Identifier: if (!routine(ops, vars, &offset)) return false; break;

            case RightBracket: {
                if (offset > 0) arrins(*ops, 0, OpReserveBytes(offset)); 
                shfree(vars);
                consume();
                return true;
            } break;

            case Eof: {
                shfree(vars);
                error_msg("COMPILATION ERROR: Expected '}' after routine body");
                return false;
            } break;

            default: {
                shfree(vars);
                printf("%s\n", display_type(get_token().type));
                error_msg("COMPILATION ERROR: Unsupported token type in routine");
                return false;
            }
        }
    }

    return false;
}

bool generate_ops(Op* ops[]) {
    while (true) {
        consume();
        switch (get_token().type) {
            case Eof: return true;
            case ParseError: return false;
            case Routine: return compile_routine_body(ops);
            default: {
                // printf("%s\n", display_type(get_token().type));
                error_msg("COMPILATION ERROR: A program file is composed by only routines");
                return false;
            }
        }
    }

    return true;
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

static void routine_call(String_Builder* out, Op op) {
    switch (op.routine_call.arg.type) {
        case Value:
            switch (op.routine_call.arg.size) {
                case Byte: sb_appendf(out, "    mov dil, %d\n", get_byte(op.routine_call.arg)); break;
                case Word: sb_appendf(out, "    mov di, %d\n", get_word(op.routine_call.arg)); break;
                case DWord: sb_appendf(out, "    mov edi, %d\n", get_dword(op.routine_call.arg)); break;
                case QWord: sb_appendf(out, "    mov rdi, %ld\n", get_qword(op.routine_call.arg)); break;
            } break;
        case Offset:
            switch (op.routine_call.arg.size) {
                case Byte: sb_appendf(out, "    mov dil, byte ptr [rbp - %zu]\n", op.routine_call.arg.offset); break;
                case Word: sb_appendf(out, "    mov di, word ptr [rbp - %zu]\n", op.routine_call.arg.offset); break;
                case DWord: sb_appendf(out, "    mov edi, dword ptr [rbp - %zu]\n", op.routine_call.arg.offset); break;
                case QWord: sb_appendf(out, "    mov rdi, qword ptr [rbp - %zu]\n", op.routine_call.arg.offset); break;
            } break;
    }

    sb_appendf(out, "    call %s\n", op.routine_call.name);
}

static void binary_operation(String_Builder* out, Op op) {
    assert(op.binop.lhs.size == op.binop.rhs.size);
    assert(op.binop.lhs.size == op.binop.offset_dst.size);

    switch (op.binop.op) {
        case Add: {
            switch (op.binop.lhs.type) {
                case Value:
                    switch (op.binop.lhs.size) {
                        case Byte: sb_appendf(out, "    mov al, %d\n", get_byte(op.binop.lhs)); break;
                        case Word: sb_appendf(out, "    mov ax, %d\n", get_word(op.binop.lhs)); break;
                        case DWord: sb_appendf(out, "    mov eax, %d\n", get_dword(op.binop.lhs)); break;
                        case QWord: sb_appendf(out, "    mov rax, %ld\n", get_qword(op.binop.lhs)); break;
                    } break;
                case Offset:
                    switch (op.binop.lhs.size) {
                        case Byte: sb_appendf(out, "    mov al, byte ptr [rbp - %zu]\n", op.binop.lhs.offset); break;
                        case Word: sb_appendf(out, "    mov ax, word ptr [rbp - %zu]\n", op.binop.lhs.offset); break;
                        case DWord: sb_appendf(out, "    mov eax, dword ptr [rbp - %zu]\n", op.binop.lhs.offset); break;
                        case QWord: sb_appendf(out, "    mov rax, qword ptr [rbp - %zu]\n", op.binop.lhs.offset); break;
                    } break;
            }

            switch (op.binop.rhs.type) {
                case Value:
                    switch (op.binop.rhs.size) {
                        case Byte: sb_appendf(out, "    add al, %d\n", get_byte(op.binop.rhs)); break;
                        case Word: sb_appendf(out, "    add ax, %d\n", get_word(op.binop.rhs)); break;
                        case DWord: sb_appendf(out, "    add eax, %d\n", get_dword(op.binop.rhs)); break;
                        case QWord: sb_appendf(out, "    add rax, %ld\n", get_qword(op.binop.rhs)); break;
                    } break;
                case Offset:
                    switch (op.binop.rhs.size) {
                        case Byte: sb_appendf(out, "    add al, byte ptr [rbp - %zu]\n", op.binop.rhs.offset); break;
                        case Word: sb_appendf(out, "    add ax, word ptr [rbp - %zu]\n", op.binop.rhs.offset); break;
                        case DWord: sb_appendf(out, "    add eax, dword ptr [rbp - %zu]\n", op.binop.rhs.offset); break;
                        case QWord: sb_appendf(out, "    add rax, qword ptr [rbp - %zu]\n", op.binop.rhs.offset); break;
                    } break;
            }

            switch (op.binop.offset_dst.type) {
                case Offset:
                    switch (op.binop.rhs.size) {
                        case Byte: sb_appendf(out, "    mov byte ptr [rbp - %zu], al\n", op.binop.offset_dst.offset); break;
                        case Word: sb_appendf(out, "    mov word ptr [rbp - %zu], ax\n", op.binop.offset_dst.offset); break;
                        case DWord: sb_appendf(out, "    mov dword ptr [rbp - %zu], eax\n", op.binop.offset_dst.offset); break;
                        case QWord: sb_appendf(out, "    mov qword ptr [rbp - %zu], rax\n", op.binop.offset_dst.offset); break;
                    } break;
                default: UNREACHABLE("");
            }

        } break;
        default: TODO("");
    } 
}

static void assign_local(String_Builder* out, Op op) {
    switch (op.assign_loc.offset_dst.type) {
        case Offset:
            switch (op.assign_loc.arg.type) {
                case Value:
                    switch (op.assign_loc.arg.size) {
                        case Byte: sb_appendf(out, "    mov byte ptr [rbp - %zu], %d\n", op.assign_loc.offset_dst.offset, get_byte(op.assign_loc.arg)); break;
                        case Word: sb_appendf(out, "    mov word ptr [rbp - %zu], %d\n", op.assign_loc.offset_dst.offset, get_word(op.assign_loc.arg)); break;
                        case DWord: sb_appendf(out, "    mov dword ptr [rbp - %zu], %d\n", op.assign_loc.offset_dst.offset, get_dword(op.assign_loc.arg)); break;
                        case QWord: sb_appendf(out, "    mov qword ptr [rbp - %zu], %ld\n", op.assign_loc.offset_dst.offset, get_qword(op.assign_loc.arg)); break;
                    } break;
                case Offset:
                    switch (op.assign_loc.arg.size) {
                        case Byte:
                            sb_appendf(out, "    mov al, byte ptr [rbp - %zu]\n", op.assign_loc.arg.offset);
                            sb_appendf(out, "    mov byte ptr [rbp - %zu], al\n", op.assign_loc.offset_dst.offset);
                            break;
                        case Word:
                            sb_appendf(out, "    mov ax, word ptr [rbp - %zu]\n", op.assign_loc.arg.offset);
                            sb_appendf(out, "    mov word ptr [rbp - %zu], ax\n", op.assign_loc.offset_dst.offset);
                            break;
                        case DWord:
                            sb_appendf(out, "    mov eax, dword ptr [rbp - %zu]\n", op.assign_loc.arg.offset);
                            sb_appendf(out, "    mov dword ptr [rbp - %zu], eax\n", op.assign_loc.offset_dst.offset);
                            break;
                        case QWord:
                            sb_appendf(out, "    mov rax, qword ptr [rbp - %zu]\n", op.assign_loc.arg.offset);
                            sb_appendf(out, "    mov qword ptr [rbp - %zu], rax\n", op.assign_loc.offset_dst.offset);
                            break;
                    } break;
            } break;
        default: UNREACHABLE("");
    }
}

static void reserve_bytes(String_Builder* out, Op op) {
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

bool generate_GAS_x86_64(String_Builder* out, Op* ops) {
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
        }
    }

    function_epilog(out);
    return true;
}

void dump_arg(const Arg* arg) {
    switch (arg->type) {
        case Value:
            printf("Argument Type: Value\n");
            switch (arg->size) {
                case Byte:
                    printf("Size: Byte\n");
                    printf("Value: %d\n", get_byte(*arg)); // Cast a int per stampare un byte
                    break;

                case Word:
                    printf("Size: Word\n");
                    printf("Value: %d\n", get_word(*arg)); // Cast a int per stampare un word
                    break;

                case DWord:
                    printf("Size: DWord\n");
                    printf("Value: %d\n", get_dword(*arg)); // Stampa un DWord (64 bit)
                    break;

                case QWord:
                    printf("Size: QWord\n");
                    printf("Value: %ld\n", get_qword(*arg)); // Stampa un QWord (64 bit)
                    break;

                default:
                    printf("Unknown Size Type\n");
                    break;
            }
            break;

        case Offset:
            printf("Argument Type: Offset\n");
            switch (arg->size) {
                case Byte:
                    printf("Size: Byte\n");
                    printf("Offset Value: %zu\n", arg->offset); // Dividi per la dimensione di un byte
                    break;

                case Word:
                    printf("Size: Word\n");
                    printf("Offset Value: %zu\n", arg->offset); // Dividi per la dimensione di un word
                    break;

                case DWord:
                    printf("Size: DWord\n");
                    printf("Offset Value: %zu\n", arg->offset); // Dividi per la dimensione di un DWord
                    break;

                case QWord:
                    printf("Size: QWord\n");
                    printf("Offset Value: %zu\n", arg->offset); // Dividi per la dimensione di un QWord
                    break;

                default:
                    printf("Unknown Size Type\n");
                    break;
            }
            break;
    }
}

void dump_op(const Op* op) {
    switch (op->type) {
        case ReserveBytes:
            printf("Operation Type: ReserveBytes\n");
            printf("Bytes Reserved: %zu\n", op->reserve_bytes.bytes);
            break;

        case AssignLocal:
            printf("Operation Type: AssignLocal\n");
            printf("Offset Destination:\n");
            dump_arg(&op->assign_loc.offset_dst);
            printf("Argument:\n");
            dump_arg(&op->assign_loc.arg);
            break;

        case RoutineCall:
            printf("Operation Type: RoutineCall\n");
            printf("Routine Name: %s\n", op->routine_call.name);
            printf("Argument:\n");
            dump_arg(&op->routine_call.arg);
            break;

        case Binary:
            printf("Operation Type: Binary\n");
            printf("Offset Destination:\n");
            dump_arg(&op->binop.offset_dst);
            printf("Binary Operation: %d\n", op->binop.op); // Stampa l'operazione binaria
            printf("Left Hand Side Argument:\n");
            dump_arg(&op->binop.lhs);
            printf("Right Hand Side Argument:\n");
            dump_arg(&op->binop.rhs);
            break;
    }
}

void dump_ops(Op *ops) {
    size_t len = arrlen(ops);

    printf("Operations: \n");
    for (size_t i = 0; i < len; ++i) {
        dump_op(ops + i);
        printf("\n");
    }

    printf("\n");
}
