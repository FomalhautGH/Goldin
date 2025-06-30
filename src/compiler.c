#include "compiler.h"
#include "lexer.h"

static Compiler comp = {0};
void init_compiler() {}
void free_compiler() {}

static bool consume() {
    return next_token();
}

static bool expect_type(TokenType type, const char* msg) {
    if (get_token().type != type) {
        error(msg);
        return false;
    }
    return true;
}

static bool consume_and_expect(TokenType type, const char* msg) {
    next_token();
    if (!expect_type(type, msg)) return false; 
    return true;
}

static bool expect_and_consume(TokenType type, const char* msg) {
    if (!expect_type(type, msg)) return false; 
    next_token();
    return true;
}

static void push_op(Op* ops[], OpType type, ...) {
    va_list args;
    va_start(args, type);
    switch (type) {
        case ReserveBytes: {
            size_t num = va_arg(args, size_t);
            arrput(*ops, ((Op) {
                .type = ReserveBytes,
                .value.num_bytes = num,
            }));
        } break;
        case StoreVar32: {
            size_t offset = va_arg(args, size_t);
            int arg = va_arg(args, int);
            arrput(*ops, ((Op) {
                .type = StoreVar32,
                .value.store_offset = offset,
                .value.store_arg = arg
            }));
        } break;
        case LoadVar32: {
            size_t offset = va_arg(args, size_t);
            arrput(*ops, ((Op) {
                .type = LoadVar32,
                .value.load_offset = offset,
            }));
        } break;
        case RoutineCall: {
            const char* name = va_arg(args, const char*);

            Arg arg = {0};
            arg.type = va_arg(args, ArgType);
            switch (arg.type) {
                case Var: {
                    size_t offset = va_arg(args, size_t);
                    arg.value.offset = offset;
                } break;
                case Literal: {
                    int val = va_arg(args, int);
                    arg.value.i32 = val;
                } break;
            }

            arrput(*ops, ((Op) {
                .type = RoutineCall,
                .value.name = name,
                .value.routine_arg = arg
            }));
        } break;
    }
    va_end(args);
}

static bool declare_var(Op* ops[], TokenType var_type, size_t* offset, VarsHashMap* vars) {
    consume_and_expect(Identifier, "COMPILATION ERROR: Expected variable name");
    const char* var_name = strdup(get_value().items);

    if (shget(vars, var_name) != -1) {
        error("COMPILATION ERROR: Redefintion of variable");
        return false;
    }

    *offset += 4;
    shput(vars, var_name, *offset);

    consume();
    if (get_type() == Equal) {
        consume_and_expect(NumberLiteral, "COMPILATION ERROR: Expected integer");
        int arg = strtol(get_value().items, NULL, 10);
        // TODO: Check errno
        push_op(ops, StoreVar32, *offset, arg);
        consume();
    }

    if (!expect_type(SemiColon, "COMPILATION ERROR: Expected ';' after variable declaration")) return false;
    return true;
}

static bool routine_call(Op* ops[], VarsHashMap* vars) {
    const char* routine_name = strdup(get_value().items);
    consume_and_expect(LeftParen, "COMPILATION ERROR: Expected '(' after routine call");

    consume();
    if (get_type() == Identifier) {
        int offset = shget(vars, get_value().items);
        if (offset == -1) {
            error("COMPILATION ERROR: Usage of undeclared variable");
            return false;
        } 
        push_op(ops, RoutineCall, routine_name, Var, offset);
        consume();
    } else if (get_type() == NumberLiteral) {
        int arg = strtol(get_value().items, NULL, 10);
        push_op(ops, RoutineCall, routine_name, Literal, arg);
        consume();
    }

    if (!expect_type(RightParen, "COMPILATION ERROR: Expected ')' after argument list")) return false;
    consume_and_expect(SemiColon, "COMPILATION ERROR: Expected ';' after variable declaration");

    return true;
}

static bool compile_routine_body(Op* ops[]) {
    size_t offset = 0;
    // TODO: Can this pointer change in functions?
    VarsHashMap* vars = NULL;
    shdefault(vars, -1);

    consume();
    if (strcmp(get_value().items, "main") != 0) {
        error("COMPILATION ERROR: Only main function supported right now");
        return false;
    }

    // TODO: function arguments
    consume_and_expect(LeftParen, "COMPILATION ERROR: Expected '(' after routine name");
    consume_and_expect(RightParen, "COMPILATION ERROR: Expected ')'");
    consume_and_expect(LeftBracket, "COMPILATION ERROR: Expected '{' after routine declaration");

    while (true) {
        consume();
        switch (get_type()) {
            // TODO: Support other types
            case VarTypei32: if (!declare_var(ops, VarTypei32, &offset, vars)) return false; break;
             // TODO: Only routines call for now and no arity checked
            case Identifier: if (!routine_call(ops, vars)) return false; break;

            case RightBracket: {
                if (offset > 0) arrins(*ops, 0, ((Op){.type = ReserveBytes, .value.num_bytes = offset})); 
                shfree(vars);
                consume();
                return true;
            } break;

            case Eof: {
                shfree(vars);
                error("COMPILATION ERROR: Expected '}' after routine body");
                return false;
            } break;

            default: {
                shfree(vars);
                printf("%s\n", display_type(get_token().type));
                error("COMPILATION ERROR: Unsupported token type in routine");
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
                error("COMPILATION ERROR: A program file is composed by only routines");
                return false;
            }
        }
    }

    return true;
}

static const char* size_to_str(VarSize size) {
    switch (size) {
        case QuadWord: { return "QuadWord"; }
        case DoubleWord: { return "DoubleWord"; }
        case Word: { return "Word"; }
        case Byte: { return "Byte"; }
        default: UNREACHABLE("");
    }
}

static void dump_op(Op op) {
    switch (op.type) {
        case ReserveBytes: {
            printf("ReserveBytes ");
            printf("{ .num_bytes = %zu }", op.value.num_bytes);
        } break;
        case StoreVar32: {
            printf("StoreVar32 ");
            printf("{ .store_offset = %zu, .arg = %d }", op.value.store_offset, op.value.store_arg);
        } break;
        case LoadVar32: {
            printf("LoadVar32 ");
            printf("{ .load_offset = %zu }", op.value.load_offset);
        } break;
        case RoutineCall: {
            printf("RoutineCall ");
            printf("{ .name = %s, .arg.value.offset = %zu }", op.value.name, op.value.routine_arg.value.offset);
        } break;
        default: UNREACHABLE("");
    }
}

void dump_ops(Op* ops) {
    size_t len = arrlen(ops);

    printf("Operations: \n");
    for (size_t i = 0; i < len; ++i) {
        dump_op(ops[i]);
        printf("\n");
    }

    printf("\n");
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
        switch (op.type) {
            case ReserveBytes: sb_appendf(out, "    sub rsp, %zu\n", op.value.num_bytes); break;
            case StoreVar32: sb_appendf(out, "    mov dword ptr [rsp + %zu], %d\n", op.value.store_offset, op.value.store_arg); break;
            case RoutineCall: {
                if (op.value.routine_arg.type == Var) {
                    sb_appendf(out, "    mov rdi, qword ptr [rsp + %zu]\n", op.value.routine_arg.value.offset);
                } else if (op.value.routine_arg.type == Literal) {
                    sb_appendf(out, "    mov rdi, %d\n", op.value.routine_arg.value.i32);
                } else {
                    UNREACHABLE("");
                }

                sb_appendf(out, "    call %s\n", op.value.name);

            } break;
            default: UNREACHABLE("");
        }
    }

    sb_appendf(out, "    mov rsp, rbp\n");
    sb_appendf(out, "    xor rax, rax\n");
    sb_appendf(out, "    pop rbp\n");
    sb_appendf(out, "    ret\n");
    return true;
}

