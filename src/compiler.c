#include "compiler.h"
#include "lexer.h"
#include "token.h"
#include <stdio.h>

// static Compiler comp = {0};
void init_compiler() {}
void free_compiler() {}

static bool consume() {
    return next_token();
}

static bool expect_type(TokenType type) {
    if (get_token().type != type) {
        error_expected(get_token().type, type);
        return false;
    }
    return true;
}

static bool consume_and_expect(TokenType type) {
    next_token();
    if (!expect_type(type)) return false; 
    return true;
}

static bool compile_expression(Op* ops[]) {
    // TODO: Check errno in convertions and overflow if literal too big
    // TODO: Pratt parser

    switch (get_type()) {
        case Identifier: break; // Variable
        case IntLiteral: TODO("Integer literals unsupported yet"); break;
        case RealLiteral: TODO("Floats unsupported yet"); break;
        case StringLiteral: TODO("String usopported yet"); break;
        default: UNREACHABLE("Unsupported token type in expression");
    }

    return true;
}

static bool declare_var(Op* ops[], TokenType var_type, size_t* offset, VarsHashMap* vars) {
    if (!consume_and_expect(Identifier)) return false;
    const char* var_name = strdup(get_value().items);

    if (shget(vars, var_name) != -1) {
        error_msg("COMPILATION ERROR: Redefinition of variable");
        return false;
    }

    size_t bytes = 0;
    TokenType lit = IntLiteral;
    switch (var_type) {
        case VarTypeu8:
        case VarTypei8: bytes = 1; break;

        case VarTypeu16:
        case VarTypei16: bytes = 2; break;

        case VarTypef32: lit = RealLiteral; TODO("Float not supported yet");
        case VarTypeu32:
        case VarTypei32: bytes = 4; break;

        case VarTypef64: lit = RealLiteral; TODO("Double not supported yet");
        case VarTypeu64:
        case VarTypei64: bytes = 8; break;

        default: UNREACHABLE("Not a valid var type");
    }

    *offset += bytes;
    shput(vars, var_name, *offset);

    consume();
    if (get_type() == Equal) {
        consume();
        if (!compile_expression(ops)) return false;
        // consume(); // TODO: Do we need it after compiler_expression implementation?
    }

    if (!expect_type(SemiColon)) return false;
    return true;
}

static bool routine_call(Op* ops[], VarsHashMap* vars) {
    // const char* routine_name = strdup(get_value().items);
    if (!consume_and_expect(LeftParen)) return false;

    consume();
    if (get_type() == Identifier) {
        int offset = shget(vars, get_value().items);
        if (offset == -1) {
            error_msg("COMPILATION ERROR: Usage of undeclared variable");
            return false;
        } 
        // push_op(ops, RoutineCall, routine_name, LocalVar, offset);
        consume();
    } else if (get_type() == IntLiteral) {
        // int arg = strtol(get_value().items, NULL, 10);
        // push_op(ops, RoutineCall, routine_name, Literal, arg);
        consume();
    }

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
            case Identifier: if (!routine_call(ops, vars)) return false; break;

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

void store_imm(String_Builder* out, Op op) {
    const char* size = "";
    sb_appendf(out, "    mov %s ptr [rsp + %zu], %d\n", size, op.store_imm.offset, op.store_imm.arg.four_bytes);
}

bool generate_GAS_x86_64(String_Builder* out, Op* ops) {
    sb_appendf(out, ".intel_syntax noprefix\n");
    sb_appendf(out, ".text\n");

    sb_appendf(out, ".globl main\n");
    sb_appendf(out, "main:\n");
    sb_appendf(out, "    push rbp\n");
    sb_appendf(out, "    mov rbp, rsp\n");

    size_t len = arrlen(ops);
    for (size_t i = 0; i < len; ++i) {
        Op op = ops[i];
        // TODO: Store only supports qwords
        switch (op.type) {
            case ReserveBytes: sb_appendf(out, "    sub rsp, %zu\n", op.reserve_bytes.bytes); break;
            case StoreImmediate: sb_appendf(out, "    mov qword ptr [rsp + %zu], %d\n", op.store_imm.offset, op.store_imm.arg.four_bytes); break;
            case StoreLocalVar: {
                sb_appendf(out, "   mov	rax, qword ptr [rbp - %zu]\n", op.store_loc.offset_src);
                sb_appendf(out, "   mov	qword ptr [rbp - %zu], rax\n", op.store_loc.offset_dst);
	
            } break;
            case RoutineCall: {
                if (op.routine_call.arg.type == LocalVar) {
                    sb_appendf(out, "    mov rdi, qword ptr [rsp + %zu]\n", op.routine_call.arg.offset);
                } else if (op.routine_call.arg.type == Literal) {
                    // TODO Support other literals
                    sb_appendf(out, "    mov rdi, %d\n", op.routine_call.arg.four_bytes);
                } else {
                    UNREACHABLE("");
                }

                sb_appendf(out, "    call %s\n", op.routine_call.name);
            } break;
            case LoadLocalVar: TODO("LoadLocalVar"); break;
            case BinaryOperation: TODO("BinaryOperation"); break;
        }
    }

    sb_appendf(out, "    mov rsp, rbp\n");
    sb_appendf(out, "    xor rax, rax\n");
    sb_appendf(out, "    pop rbp\n");
    sb_appendf(out, "    ret\n");
    return true;
}

static void dump_op(Op op) {
    printf("Type: %d", op.type);
}

void dump_ops(Op *ops) {
    size_t len = arrlen(ops);

    printf("Operations: \n");
    for (size_t i = 0; i < len; ++i) {
        dump_op(ops[i]);
        printf("\n");
    }

    printf("\n");
}
