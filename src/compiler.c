#include "lexer.h"
#include "token.h"
#include "compiler.h"

static Compiler comp = {0};

static bool statement();
static bool block_statement();
static bool while_statement();
static bool compile_expression(Arg* arg);
static bool compile_expression_wrapped(Arg* arg, TokenType min_binding);

const char* display_op(Op op) {
    switch (op.type) {
        case AssignLocal: return "AssignLocal";
        case NewRoutine: return "Routine";
        case RtReturn: return "Return";
        case RoutineCall: return "RoutineCall";
        case Binary: return "Binary";
        case Label: return "Label";
        case JumpIfNot: return "JumpIfNot";
        case Jump: return "Jump";
        default: UNREACHABLE("");
    }
}

static size_t push_op(Op op) {
    size_t index = arrlenu(comp.ops);
    arrpush(comp.ops, op);
    return index;
}

static size_t push_label_op() {
    size_t index = comp.label_index;
    push_op(OpLabel(index));

    comp.label_index += 1;

    return index;
}

static Token consume() {
    Token res = get_token();
    next_token(); // check bool
    return res;
}

static bool expect_type(TokenType type) {
    if (get_type() != type) {
        error_expected(type, get_type());
        return false;
    }
    return true;
}

static bool expect_types(TokenType types[], size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (get_type() == types[i]) return true;
    }
    // TODO: Error message
    return false;
}

static bool expect_var_type() {
    TokenType var_types[] = {
        VarTypei8,
        VarTypei16,
        VarTypei32,
        VarTypei64,
        VarTypeu8,
        VarTypeu16,
        VarTypeu32,
        VarTypeu64,
        VarTypef32,
        VarTypef64
    };

    bool result = expect_types(var_types, sizeof(var_types) / sizeof(TokenType));
    if (!result) error_msg("COMPILATION ERROR: Expected a var type");
    return result;
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

static bool is_eof() {
    return get_type() == Eof;
}

static Arg find_local_var(const char* name) {
    size_t last = arrlenu(comp.local_vars) - 1;

    for (long i = last; i >= 0; --i) {
        long index = shgeti(comp.local_vars[i], name);
        if (index != -1) return comp.local_vars[i][index].value;
    }

    return (Arg){0};
}

static bool is_var_signed(TokenType var_type) {
    switch (var_type) {
        case VarTypeu8:
        case VarTypeu16:
        case VarTypeu32:
        case VarTypeu64: return false;

        case VarTypei8: 
        case VarTypei16:
        case VarTypei32:
        case VarTypei64: return true;

        case VarTypef32: 
        case VarTypef64: UNREACHABLE("Tecnically true");

        default: UNREACHABLE("Not a valid var type");
    }
}

static Size get_var_size(TokenType var_type) {
    Size size = 0;

    switch (var_type) {
        case VarTypeu8:
        case VarTypei8: size = Byte; break;

        case VarTypeu16:
        case VarTypei16: size = Word; break;

        case VarTypef32: 
        case VarTypeu32:
        case VarTypei32: size = DWord; break;

        case VarTypef64: 
        case VarTypeu64:
        case VarTypei64: size = QWord; break;
        default: UNREACHABLE("Not a valid var type");
    }

    return size;
}

static void alloc_size(Size size) {
    switch (size) {
        case Byte: comp.position += 1; break;
        case Word: comp.position += 2; break;
        case DWord: comp.position += 4; break;
        case QWord: comp.position += 8; break;
        default: UNREACHABLE("Invalid Arg size");
    }
}

static bool declare_variable(TokenType var_type, const char* name) {
    Size size = get_var_size(var_type);
    alloc_size(size);

    Arg var = {
        .position = comp.position,
        .type = Position,
        .size = size,
        .is_signed = is_var_signed(var_type)
    };

    size_t last = arrlenu(comp.local_vars) - 1;
    shput(comp.local_vars[last], name, var);
    return true;
}

static void print_current_type() {
    printf("%s\n", display_type(get_type()));
}

static bool int_literal_to_arg(Arg* arg) {
    // TODO: Check errno in convertions and overflow if literal too big
    int64_t value = strtol(get_value().items, NULL, 10);

    *arg = (Arg) {
        .size = QWord,
        .type = Value,
        .is_signed = true
    };

    memcpy(&arg->buffer, &value, sizeof(int64_t));

    consume(); // Consume IntLiteral
    return true;
}

static bool routine_call(Arg* arg, const char* name) {
    Arg temp_arg = {0};
    Arg* args = NULL;
    consume();

    while (!is_eof() && get_type() != RightParen) {
        // TODO: default arg size should be of the formal param type
        temp_arg.size = QWord;
        if (!compile_expression(&temp_arg)) return false;
        arrpush(args, temp_arg);

        switch (get_type()) {
            case RightParen: continue;
            case Comma: consume(); continue;
            default:
                print_current_type();
                error_msg("COMPILATION ERROR: Unknown Token in routine arguments");
                return false;
        }
    }

    // TODO: remove this
    if (arrlenu(args) > X86_64_LINUX_CALL_REGISTERS_NUM) {
        error_msg("COMPILATION ERROR: We only support 6 arguments for now");
        return false;
    }

    push_op(OpRoutineCall(name, args));
    if (!expect_and_consume(RightParen)) return false;
    if (arg) {
        arg->type = ReturnVal;
        arg->size = QWord;
    }
    return true;
}

static bool identifier_expression(Arg* arg) {
    const char* name = strdup(get_value().items);
    consume(); // Consume Identifier
    
    if (get_type() == LeftParen) {
        if (!routine_call(arg, name)) return false;
        return true;
    }

    Arg var = find_local_var(name);
    if (var.position == 0) {
        error_msg("COMPILATION ERROR: Usage of undefined variable");
        return false;
    }
    
    *arg = (Arg) {
        .type = Position,
        .size = var.size,
        .position = var.position,
        .is_signed = var.is_signed
    };

    return true;
}

static bool compile_binop(Arg* arg) {
    TokenType op_type = get_type();
    Arg rhs = {0};

    consume();
    if (!compile_expression_wrapped(&rhs, op_type)) return false;

    Binop binop = 0;
    Size size = max(arg->size, rhs.size);
    switch (op_type) {
        case Plus: binop = Add; break;
        case Minus: binop = Sub; break;
        case Star: binop = Mul; break;
        case Slash: binop = Div; break;
        case EqualEqual: size = Byte; binop = Eq; break;
        case Less: size = Byte; binop = Lt; break;
        case LessEqual: size = Byte; binop = Le; break;
        case Greater: size = Byte; binop = Gt; break;
        case GreaterEqual: size = Byte; binop = Ge; break;
        case BangEqual: size = Byte; binop = Ne; break;
        default: UNREACHABLE("");
    }

    alloc_size(size);
    
    Arg dst = {
        .type = Position,
        .size = size,
        .position = comp.position,
        .is_signed = false // TODO: do not hardcode this
    };

    push_op(OpBinary(dst, binop, *arg, rhs));

    *arg = (Arg) {
        .type = Position,
        .size = size,
        .position = comp.position,
        .is_signed = false // TODO: do not hardcode this
    };

    return true;
}

static bool string_literal_to_arg(Arg* arg) {
    *arg = (Arg) {
        .type = Offset,
        .size = QWord,
        .position = arrlenu(comp.static_data),
        .is_signed = false
    };

    Arg data = {
        .size = QWord,
        .type = Offset,
        .is_signed = false,
        .string = strdup(get_value().items)
    };

    arrpush(comp.static_data, data);

    consume(); // Consume StringLiteral
    return true;
}

static bool compile_primary_expression(Arg* arg) {
    switch (get_type()) {
        case Identifier: return identifier_expression(arg);
        case IntLiteral: return int_literal_to_arg(arg);
        case StringLiteral: return string_literal_to_arg(arg);
        case LeftParen: TODO("Groupings unsupported yet"); break;
        case RealLiteral: TODO("Floats unsupported yet"); break;
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
            case Star:
            case GreaterEqual:
            case Greater:
            case EqualEqual:
            case BangEqual:
            case LessEqual:
            case Less: if (!compile_binop(arg)) return false; break;
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

static bool assignment(const char* name) {
    consume(); // Consume Equal

    Arg var = find_local_var(name);
    if (var.position == 0) {
        error_msg("COMPILATION ERROR: Trying to assign to a non existing variable");
        return false;
    }

    Arg arg = {0};
    arg.size = var.size;
    if (!compile_expression(&arg)) return false;

    Arg dst = {
        .type = Position,
        .size = var.size,
        .position = var.position,
        .is_signed = var.is_signed
    };
    push_op(OpAssignLocal(dst, arg));

    free((void*)name);
    return true;
}

static bool variable_initialization(TokenType var_type) {
    consume(); // Consume Equals

    Arg arg = {0};
    Arg dst = {
        .size = get_var_size(var_type),
        .type = Position,
        .position = 0,
        .is_signed = is_var_signed(var_type)
    };

    size_t current_position = comp.position;
    if (!compile_expression(&arg)) return false;
    dst.position = current_position;
    push_op(OpAssignLocal(dst, arg));

    if (!expect_and_consume(SemiColon)) return false;
    return true;
}

static bool variable_declaration() {
    TokenType var_type = get_type();

    consume();
    const char* var_name = expect_consume_id_and_get_string();
    if (find_local_var(var_name).position != 0) {
        error_msg("COMPILATION ERROR: Redefinition of variable");
        free((void*)var_name);
        return false;
    }

    if (!declare_variable(var_type, var_name)) return false;

    switch (get_type()) {
        case Equal: return variable_initialization(var_type);
        case SemiColon: consume(); return true;
        default: {
            error_msg("COMPILATION ERROR: Expected ';' after variable declaration");
        } return false;
    }

    UNREACHABLE("");
}

static bool identifier_statement() {
    const char* name = expect_consume_id_and_get_string();

    switch (get_type()) {
        case SemiColon: return true;
        case Equal: return assignment(name);
        case LeftParen: return routine_call(NULL, name); 
        default: error_msg("COMPILATION ERROR: Unexpected token after identifier"); return false;
    }

    UNREACHABLE("");
}

static bool return_statement() {
    consume(); // Consume Return

    Arg arg = {0};
    compile_expression(&arg);
    push_op(OpReturn(arg));

    comp.returned = true;
    return true;
}

static bool if_statement() {
    Arg cond = {0};
    consume();

    if (!expect_and_consume(LeftParen)) return false;
    if (!compile_expression(&cond)) return false;
    if (!expect_and_consume(RightParen)) return false;

    size_t end_if_block = push_op(OpJumpIfNot(0, cond));
    if (!statement()) return false;

    if (get_type() == Else) {
        consume();
        size_t end_else_block = push_op(OpJump(0));
        comp.ops[end_if_block].jump_if_not.label = push_label_op();
        if (!statement()) return false;
        comp.ops[end_else_block].jump.label = push_label_op();
    } else {
        comp.ops[end_if_block].jump_if_not.label = push_label_op();
    }

    return true;
}

static bool statement() {
    switch (get_type()) {
        case SemiColon: consume(); return true;
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

        case Identifier: if (!identifier_statement()) return false; break;
        case While: if (!while_statement()) return false; break;
        case LeftBracket: if (!block_statement()) return false; break;
        case Return: if (!return_statement()) return false; break;
        case If: if (!if_statement()) return false; break;

        case Eof:
            error_msg("COMPILATION ERROR: Expected statement");
            return false;

        default:
            print_current_type();
            error_msg("COMPILATION ERROR: Unsupported token type in statement");
            return false;
    }

    return true;
}

static bool block_statement() {
    if (!expect_and_consume(LeftBracket)) return false;
    arrpush(comp.local_vars, NULL);

    while (!is_eof() && get_type() != RightBracket) {
        if (!statement()) return false;
    }

    arrpop(comp.local_vars);
    if (!expect_and_consume(RightBracket)) return false;
    return true;
}

static bool while_statement() {
    Arg cond = {0};
    cond.size = Byte;

    consume();
    if (!expect_and_consume(LeftParen)) return false;

    size_t start_loop = push_label_op();
    if (!compile_expression(&cond)) return false;
    if (!expect_and_consume(RightParen)) return false;
    size_t jmpifnot = push_op(OpJumpIfNot(0, cond));
    if (!block_statement()) return false;

    push_op(OpJump(start_loop));
    comp.ops[jmpifnot].jump_if_not.label = push_label_op();

    return true;
}

static bool routine_argument(Arg* args[]) {
    TokenType var_type = get_type();
    consume(); // Consume VarType

    const char* name = strdup(get_value().items);
    consume(); // Consume Identifier
    declare_variable(var_type, name);

    Arg arg = {
        .size = get_var_size(var_type),
        .type = Position,
        .position = comp.position,
        .is_signed = is_var_signed(var_type)
    };

    arrpush(*args, arg);
    return true;
}

static bool compile_routine_arguments(Arg* args[]) {
    if (!expect_and_consume(LeftParen)) return false;
    while (get_type() != RightParen) {
        if (!routine_argument(args)) return false;
        if (get_type() == Comma) consume(); 
    }
    if (!expect_and_consume(RightParen)) return false;
    return true;
}

static bool compile_routine_body() {
    if (!expect_and_consume(LeftBracket)) return false;
    while (!is_eof() && get_type() != RightBracket) {
        if (!statement()) return false;
    }
    if (!expect_and_consume(RightBracket)) return false;
    return true;
}

static bool compile_routine() {
    consume(); // Consume Routine
    if (!expect_type(Identifier)) return false;
    const char* routine_name = strdup(get_value().items);
    consume(); // Consume Identifier

    size_t rt = push_op(OpNewRoutine(routine_name, 0, NULL));

    size_t prev_pos = comp.position;
    comp.position = 0;
    arrpush(comp.local_vars, NULL);

    Arg* args = NULL;
    if (!compile_routine_arguments(&args)) return false;
    if (!compile_routine_body()) return false;

    // TODO: support the case in which the return is generated but not
    // executed, like in a if statement
    if (!comp.returned) push_op(OpReturn((Arg){0}));
    else comp.returned = false;

    comp.ops[rt].new_routine.bytes = comp.position;
    comp.ops[rt].new_routine.args = args;

    comp.position = prev_pos;
    arrpop(comp.local_vars);
    return true;
}

void init_compiler() {
    comp.position = 0;
    comp.label_index = 0;
    comp.static_data = NULL;
    comp.ops = NULL;
    comp.local_vars = NULL;
    comp.returned = false;
}

void free_compiler() {
    for (size_t i = 0; i < arrlenu(comp.local_vars); ++i) shfree(comp.local_vars[i]);
    arrfree(comp.static_data);
    arrfree(comp.ops);
    arrfree(comp.local_vars);
}

Op* get_ops() {
    return comp.ops;
}

Arg* get_data() {
    return comp.static_data;
}

bool generate_ops() {
    consume();

    while (true) {
        Token current_token = get_token();
        switch (current_token.type) {
            case Eof: return true;
            case ParseError: return false;
            case Routine: if (!compile_routine()) return false; break;
            default: {
                error_msg("COMPILATION ERROR: A program file is composed by only routines");
                return false;
            }
        }
    }

    return true;
}
