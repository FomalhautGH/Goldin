#include "lexer.h"
#include "token.h"
#include "compiler.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static Compiler comp = {0};

static bool while_loop();

static size_t push_op(Op op) {
    size_t index = arrlenu(comp.ops);
    arrput(comp.ops, op);
    return index;
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

static bool expect_and_consume(TokenType type) {
    if (!expect_type(type)) return false; 
    next_token();
    return true;
}

static void int_literal_to_arg(Arg* arg) {
    // TODO: Check errno in convertions and overflow if literal too big
    int32_t value = strtol(get_value().items, NULL, 10);
    arg->size = DWord;
    arg->type = Value;
    memcpy(&arg->buffer, &value, sizeof(int32_t));
}

static void var_id_to_arg(Arg* arg) {
    const char* name = get_value().items;
    size_t offset = shget(comp.vars, name);
    offset_arg(arg, DWord, offset);
}

static bool compile_primary_expression(Arg* arg) {

    switch (get_type()) {
        case Identifier: var_id_to_arg(arg); consume(); break;
        case IntLiteral: int_literal_to_arg(arg); consume(); break;
        case LeftParen: TODO("Groupings unsupported yet"); break;
        case RealLiteral: TODO("Floats unsupported yet"); break;
        case StringLiteral: TODO("String usopported yet"); break;
        default: UNREACHABLE("Unsupported token type in primary expression");
    }

    return true;
}

// TODO: change tokentype to a type that represents binding powers more effectively
static bool compile_expression_wrapped(Arg* arg, TokenType min_binding) {
    // TODO: Pratt parser?
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
                push_op(OpBinary(dst, Add, *arg, rhs));

                offset_arg(arg, DWord, comp.offset);
            } break;
            case Minus: {
                consume();
                Arg rhs = {0};
                compile_expression_wrapped(&rhs, Minus);

                // TODO: offset needs to change based off the type of the expression not simply 4
                comp.offset += 4;

                Arg dst = {0};
                offset_arg(&dst, DWord, comp.offset);
                push_op(OpBinary(dst, Sub, *arg, rhs));

                offset_arg(arg, DWord, comp.offset);
            } break;
            case Star: {
                consume();
                Arg rhs = {0};
                compile_expression_wrapped(&rhs, Star);

                // TODO: offset needs to change based off the type of the expression not simply 4
                comp.offset += 4;

                Arg dst = {0};
                offset_arg(&dst, DWord, comp.offset);
                push_op(OpBinary(dst, Mul, *arg, rhs));

                offset_arg(arg, DWord, comp.offset);
            } break;
            case Less: {
                consume();
                Arg rhs = {0};
                compile_expression_wrapped(&rhs, Less);

                // TODO: offset needs to change based off the type of the expression not simply 4
                comp.offset += 4;

                Arg dst = {0};
                offset_arg(&dst, Byte, comp.offset);
                push_op(OpBinary(dst, LessThan, *arg, rhs));

                offset_arg(arg, Byte, comp.offset);
            } break;
            case Slash: TODO("Unsupported div op"); break;
            default: goto end_expr;
        }
    }

end_expr:
    return true;
}

static bool compile_expression(Arg* arg) {
    return compile_expression_wrapped(arg, 0);
}

static bool variable_declaration(TokenType var_type) {
    if (!consume_and_expect(Identifier)) return false;
    const char* var_name = strdup(get_value().items);

    if (shget(comp.vars, var_name) != -1) {
        error_msg("COMPILATION ERROR: Redefinition of variable");
        return false;
    }

    switch (var_type) {
        case VarTypeu8:
        case VarTypei8: comp.offset += 1; break;

        case VarTypeu16:
        case VarTypei16: comp.offset += 2; break;
        case VarTypef32: TODO("Float not supported yet");
        case VarTypeu32:
        case VarTypei32: comp.offset += 4; break;

        case VarTypef64: TODO("Double not supported yet");
        case VarTypeu64:
        case VarTypei64: comp.offset += 8; break;

        default: UNREACHABLE("Not a valid var type");
    }

    shput(comp.vars, var_name, comp.offset);

    consume();
    if (get_type() == Equal) {
        consume();
        size_t current_offset = comp.offset;

        Arg arg = {0};
        if (!compile_expression(&arg)) return false;

        Arg dst = {0};
        offset_arg(&dst, DWord, current_offset);
        push_op(OpAssignLocal(dst, arg));
    }

    if (!expect_and_consume(SemiColon)) return false;
    return true;
}

static bool routine(const char* name) {
    consume(); // consume '('

    Arg arg = {0};
    if (!compile_expression(&arg)) return false;
    push_op(OpRoutineCall(name, arg));

    if (!expect_type(RightParen)) return false;
    if (!consume_and_expect(SemiColon)) return false;

    return true;
}

static bool assignment(const char* name) {
    consume(); // consume '='

    Arg arg = {0};
    if (!compile_expression(&arg)) return false;

    Arg dst = {0};
    size_t offset = shget(comp.vars, name);
    offset_arg(&dst, DWord, offset);
    push_op(OpAssignLocal(dst, arg));
    return true;
}

static void reserve_bytes() {
    if (comp.offset > 0) arrins(comp.ops, 0, OpReserveBytes(comp.offset)); 
}

static bool identifier() {
    const char* name = strdup(get_value().items);
    consume();

    switch (get_type()) {
        case Equal: return assignment(name);
        case LeftParen: return routine(name); 
        case SemiColon: consume(); break;
        default: error_msg("COMPILATION ERROR: Unexpected token after identifier"); return false;
    }

    return true;
}

static bool statement() {
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
        case VarTypef64: if (!variable_declaration(get_type())) return false; break;

        case Identifier: if (!identifier()) return false; break;
        case While: if (!while_loop()) return false; break;
        case SemiColon: consume(); return true;

        case Eof:
            error_msg("COMPILATION ERROR: Expected ';' after statement");
            return false;

        default:
            printf("%s\n", display_type(get_token().type));
            error_msg("COMPILATION ERROR: Unsupported token type in statement");
            return false;
                 
    }

    return true;
}

static bool block() {
    if (!expect_and_consume(LeftBracket)) return false;

    while (get_type() != RightBracket) {
        if (!statement()) return false;
    }

    if (!expect_and_consume(RightBracket)) return false;
    return true;
}

static size_t push_label_op() {
    size_t index = comp.label_index;
    push_op(OpLabel(index));

    comp.label_index += 1;

    return index;
}

static bool while_loop() {
    if (!consume_and_expect(LeftParen)) return false;
    consume();

    size_t start_loop = push_label_op();
    Arg cond = {0};
    if (!compile_expression(&cond)) return false;
    if (!expect_and_consume(RightParen)) return false;
    size_t jmpifnot = push_op(OpJumpIfNot(0, cond));
    if (!block()) return false;

    push_op(OpJump(start_loop));
    comp.ops[jmpifnot].jump_if_not.label = push_label_op();

    return true;
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
    consume();
    if (!block()) return false;

    reserve_bytes();
    return true;
}

void init_compiler() {
    comp.offset = 0;
    comp.label_index = 0;
    comp.ops = NULL;

    // TODO: Change hashtable depending on the scope
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
