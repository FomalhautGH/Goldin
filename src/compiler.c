#include "lexer.h"
#include "token.h"
#include <stddef.h>
#include <stdio.h>
#include "compiler.h"

static Compiler comp = {0};

static bool block();
static bool while_loop();
static bool compile_expression_wrapped(Arg* arg, TokenType min_binding);

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

static Token consume() {
    Token res = get_token();
    next_token(); // check bool
    return res;
}

static bool expect_type(TokenType type) {
    if (get_token().type != type) {
        error_expected(type, get_token().type);
        return false;
    }
    return true;
}

static bool expect_and_consume(TokenType type) {
    if (!expect_type(type)) return false; 
    next_token();
    return true;
}

static const char* expect_consume_id_and_get_string() {
    if (!expect_type(Identifier)) return NULL; 
    const char* result = strdup(get_value().items);
    next_token();
    return result;
}

static long find_var_offset(const char* name) {
    size_t last = arrlenu(comp.local_vars) - 1;

    for (long i = last; i >= 0; --i) {
        long index = shgeti(comp.local_vars[i], name);
        if (index != -1) return comp.local_vars[i][index].value;
    }

    return -1;
}

static bool declare_variable(TokenType var_type, const char* name) {
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

    size_t last = arrlenu(comp.local_vars) - 1;
    shput(comp.local_vars[last], name, comp.offset);
    return true;
}

static void print_current_type() {
    printf("%s\n", display_type(get_type()));
}

static void int_literal_to_arg(Arg* arg) {
    // TODO: Check errno in convertions and overflow if literal too big
    int32_t value = strtol(get_value().items, NULL, 10);
    arg->size = DWord;
    arg->type = Value;
    memcpy(&arg->buffer, &value, sizeof(int32_t));
    consume();
}

static bool var_id_to_arg(Arg* arg) {
    const char* name = get_value().items;
    long offset = find_var_offset(name);
    
    if (offset == -1) {
        error_msg("COMPILATION ERROR: Usage of undefined variable");
        return false;
    }
    
    offset_arg(arg, DWord, offset);
    consume();
    return true;
}

static bool compile_binop(Arg* arg, int size) {
    Arg rhs = {0};
    Arg dst = {0};
    TokenType type = get_type();

    consume();

    Binop binop = 0;
    switch (type) {
        case Plus: binop = Add; break;
        case Minus: binop = Sub; break;
        case Star: binop = Mul; break;
        case Slash: binop = Div; break;
        case Less: binop = LessThan; break;
        default: UNREACHABLE("");
    }

    if (!compile_expression_wrapped(&rhs, type)) return false;

    // TODO: offset needs to change based off the type of the expression not simply 4
    comp.offset += 4;

    offset_arg(&dst, size, comp.offset);
    push_op(OpBinary(dst, binop, *arg, rhs));

    offset_arg(arg, size, comp.offset);
    return true;
}

static bool compile_primary_expression(Arg* arg) {
    switch (get_type()) {
        case Identifier: return var_id_to_arg(arg);
        case IntLiteral: int_literal_to_arg(arg); break;
        case LeftParen: TODO("Groupings unsupported yet"); break;
        case RealLiteral: TODO("Floats unsupported yet"); break;
        case StringLiteral: TODO("String usopported yet"); break;
        default: error_msg("COMPILATION ERROR: Expected expression"); return false;
    }
    return true;
}

// TODO: change tokentype to a type that represents binding powers more effectively
static bool compile_expression_wrapped(Arg* arg, TokenType min_binding) {
    if (!compile_primary_expression(arg)) return false;

    while (get_type() > min_binding) {
        switch (get_type()) {
            case Plus: 
            case Minus: 
            case Star: return compile_binop(arg, DWord);
            case Less: return compile_binop(arg, Byte);
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

static bool variable_initialization() {
    Arg arg = {0};
    Arg dst = {0};

    size_t current_offset = comp.offset;
    if (!compile_expression(&arg)) return false;
    offset_arg(&dst, DWord, current_offset);
    push_op(OpAssignLocal(dst, arg));

    expect_and_consume(SemiColon);
    return true;
}

static bool variable_declaration() {
    TokenType var_type = get_type();

    consume();
    const char* var_name = expect_consume_id_and_get_string();
    if (find_var_offset(var_name) != -1) {
        error_msg("COMPILATION ERROR: Redefinition of variable");
        free((void*)var_name);
        return false;
    }

    if (!declare_variable(var_type, var_name)) return false;

    switch (consume().type) {
        case Equal: return variable_initialization();
        case SemiColon: return true;
        default: {
            error_msg("COMPILATION ERROR: Expected ';' after variable declaration");
        } return false;
    }

    UNREACHABLE("");
}

static bool routine_call(const char* name) {
    Arg arg = {0};

    if (!compile_expression(&arg)) return false;
    push_op(OpRoutineCall(name, arg));

    if (!expect_and_consume(RightParen)) return false;
    if (!expect_and_consume(SemiColon)) return false;
    return true;
}

static bool assignment(const char* name) {
    Arg arg = {0};
    Arg dst = {0};

    if (!compile_expression(&arg)) return false;

    long offset = find_var_offset(name);
    if (offset == -1) {
        error_msg("COMPILATION ERROR: Trying to assign to a non existing variable");
        return false;
    }

    offset_arg(&dst, DWord, offset);
    push_op(OpAssignLocal(dst, arg));
    free((void*)name);
    return true;
}

static bool identifier() {
    const char* name = expect_consume_id_and_get_string();

    switch (consume().type) {
        case Equal: return assignment(name);
        case LeftParen: return routine_call(name); 
        case SemiColon: break;
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
        case VarTypef64: if (!variable_declaration()) return false; break;

        case Identifier: if (!identifier()) return false; break;
        case While: if (!while_loop()) return false; break;
        case SemiColon: consume(); return true; break;
        case LeftBracket: if (!block()) return false; break;

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
    arrput(comp.local_vars, NULL);

    while (get_type() != RightBracket) {
        if (!statement()) return false;
    }

    arrpop(comp.local_vars);
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
    Arg cond = {0};

    consume();
    if (!expect_and_consume(LeftParen)) return false;

    size_t start_loop = push_label_op();
    if (!compile_expression(&cond)) return false;
    if (!expect_and_consume(RightParen)) return false;
    size_t jmpifnot = push_op(OpJumpIfNot(0, cond));
    if (!block()) return false;

    push_op(OpJump(start_loop));
    comp.ops[jmpifnot].jump_if_not.label = push_label_op();

    return true;
}

static bool compile_routine_body() {
    if (strcmp(get_value().items, "main") != 0) {
        error_msg("COMPILATION ERROR: Only main function supported right now");
        return false;
    }

    consume();

    size_t reserve = push_op(OpReserveBytes(0));

    if (!expect_and_consume(LeftParen)) return false;
    // TODO: function arguments
    if (!expect_and_consume(RightParen)) return false;
    if (!block()) return false;

    comp.ops[reserve].reserve_bytes.bytes = comp.offset;
    return true;
}

void init_compiler() {
    comp.offset = 0;
    comp.label_index = 0;
    comp.ops = NULL;
    comp.local_vars = NULL;
}

void free_compiler() {
    for (size_t i = 0; i < arrlenu(comp.local_vars); ++i) shfree(comp.local_vars[i]);
    arrfree(comp.local_vars);
    arrfree(comp.ops);
}

Op* get_ops() {
    return comp.ops;
}

bool generate_ops() {
    consume();

    while (true) {
        Token current_token = consume();
        switch (current_token.type) {
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
