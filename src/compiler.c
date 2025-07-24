#include "lexer.h"
#include "token.h"
#include "compiler.h"

static Compiler comp = {0};

static void add_op(Op op) {
    arrput(comp.ops, op);
}

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

static bool compile_primary_expression(Arg* arg) {
    // TODO: Check errno in convertions and overflow if literal too big
    // TODO: Pratt parser

    switch (get_type()) {
        case Identifier: {
            const char* name = get_value().items;
            size_t offset = shget(comp.vars, name);
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
static bool compile_expression_wrapped(Arg* arg, TokenType min_binding) {
    compile_primary_expression(arg);

    while (get_type() > min_binding) {
        switch (get_type()) {
            case Plus: {
                consume();
                Arg rhs = {0};
                compile_expression_wrapped(&rhs, Plus);

                // TODO: offset needs to change based off the type of the expression not simply 4
                comp.offset += 4;

                Arg dst = {0};
                offset_arg(&dst, DWord, comp.offset);
                add_op(OpBinary(dst, Add, *arg, rhs));

                offset_arg(arg, DWord, comp.offset);
            } break;
            case Minus: TODO("Unsupported minus op"); break;
            case Slash: TODO("Unsupported div op"); break;
            case Star: TODO("Unsupported mul op"); break;
            default: goto end_expr;
        }
    }

end_expr:
    return true;
}

static bool compile_expression(Arg* arg) {
    return compile_expression_wrapped(arg, 0);
}

static bool declare_var(TokenType var_type) {
    if (!consume_and_expect(Identifier)) return false;
    const char* var_name = strdup(get_value().items);

    if (shget(comp.vars, var_name) != -1) {
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

    comp.offset += bytes;
    shput(comp.vars, var_name, comp.offset);

    consume();
    if (get_type() == Equal) {
        consume();

        Arg arg = {0};
        if (!compile_expression(&arg)) return false;

        Arg dst = {0};
        offset_arg(&dst, DWord, comp.offset);
        add_op(OpAssignLocal(dst, arg));
    }

    if (!expect_type(SemiColon)) return false;
    return true;
}

static bool routine() {
    const char* name = strdup(get_value().items);
    if (!consume_and_expect(LeftParen)) return false;
    consume();

    Arg arg = {0};
    if (!compile_expression(&arg)) return false;
    add_op(OpRoutineCall(name, arg));

    if (!expect_type(RightParen)) return false;
    if (!consume_and_expect(SemiColon)) return false;

    return true;
}

static void reserve_bytes() {
    if (comp.offset > 0) arrins(comp.ops, 0, OpReserveBytes(comp.offset)); 
}

static bool compile_routine_body() {
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
            case VarTypef64: if (!declare_var(get_type())) return false; break;

             // TODO: Only routines call for now and no arity checked
            case Identifier: if (!routine()) return false; break;

            case RightBracket: {
                reserve_bytes();
                consume();
                return true;
            } break;

            case Eof: {
                error_msg("COMPILATION ERROR: Expected '}' after routine body");
                return false;
            } break;

            default: {
                printf("%s\n", display_type(get_token().type));
                error_msg("COMPILATION ERROR: Unsupported token type in routine");
                return false;
            }
        }
    }

    return false;
}

void init_compiler() {
    comp.offset = 0;
    comp.ops = NULL;

    // TODO: Can this pointer change in functions?
    comp.vars = NULL;
    shdefault(comp.vars, -1);
}

void free_compiler() {
    shfree(comp.vars);
    arrfree(comp.ops);
}

Op* get_ops() {
    return comp.ops;
}

bool generate_ops() {
    while (true) {
        consume();
        switch (get_token().type) {
            case Eof: return true;
            case ParseError: return false;
            case Routine: return compile_routine_body();
            default: {
                error_msg("COMPILATION ERROR: A program file is composed by only routines");
                return false;
            }
        }
    }

    return true;
}
