#include "token.h"
#include "lexer.h"
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    QuadWord,
    DoubleWord,
    Word,
    Byte
} VarSize;

typedef enum {
    Var,
    Literal
} ArgType;

typedef union {
    int i32;
    float f32; // Literal
    size_t offset; // Var
} ArgValue;

typedef struct {
    ArgType type;
    ArgValue value;
} Arg;

typedef enum {
    ReserveBytes,
    LoadVar32,
    StoreVar32,
    RoutineCall 
} OpType;

typedef union {
    size_t num_bytes;                                             // ReserveBytes
    size_t load_offset;                                           // LoadVar32
    struct { VarSize size; size_t store_offset; int arg_store; }; // StoreVar32
    struct { const char* name; Arg arg; };                        // RoutineCall
} OpValue;

typedef struct {
    OpType type;
    OpValue value;
} Op;

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define NOB_STRIP_PREFIX
#include "../nob.h"

typedef struct {
    const char* key;
    int value;
} Vars;

typedef struct {
} Compiler;

// static Compiler comp = {0};
static void init_compiler() {}
static void free_compiler() {}

static bool consume() {
    // printf("%s\n", display_type(get_token().type));
    return next_token();
}

static bool expect_type(TokenType type, const char* msg) {
    if (get_token().type != type) {
        error(msg);
        return false;
    }
    return true;
}

static bool expect_and_consume(TokenType type, const char* msg) {
    if (!expect_type(type, msg)) return false; 
    next_token();
    return true;
}

#define expect_and_consume_ret(type, msg)                 \
    do {                                                  \
        if (!expect_and_consume(type, msg)) return false; \
    } while (0) 

#define expect_ret(type, msg)                             \
    do {                                                  \
        if (!expect_type(type, msg)) return false;        \
    } while (0) 

static bool compile_routine_body(Op* ops[]) {
    Vars* vars = NULL;
    shdefault(vars, -1);
    size_t offset = 0;

    consume(); // Routine name
    if (strcmp(get_value().items, "main") != 0) {
        error("COMPILATION ERROR: Only main function supported right now");
        return false;
    }

    consume();
    if (!expect_type(LeftParen, "COMPILATION ERROR: Expected '(' after routine name")) return false;
    // TODO: function arguments
    consume();
    if (!expect_type(RightParen, "COMPILATION ERROR: Expected ')'")) return false;

    consume();
    if (!expect_type(LeftBracket, "COMPILATION ERROR: Expected '{' after routine declaration")) return false;

    while (true) {
        consume();
        switch (get_type()) {
            case RightBracket: {
                if (offset > 0) arrins(*ops, 0, ((Op){.type = ReserveBytes, .value.num_bytes = offset})); 
                consume();
                shfree(vars);
                return true;
            }
            case VarTypei32: {
                consume();
                if (!expect_type(Identifier, "COMPILATION ERROR: Expected variable name")) return false;
                const char* var_name = strdup(get_value().items);

                if (shget(vars, var_name) != -1) {
                    error("COMPILATION ERROR: Redefintion of variable");
                    return false;
                }

                offset += 4;
                shput(vars, var_name, offset);

                consume();
                if (get_type() == Equal) {
                    consume();
                    if (!expect_type(NumberLiteral, "COMPILATION ERROR: Expected integer")) return false;
                    int arg = strtol(get_value().items, NULL, 10);
                    // TODO: Check errno

                    arrput(*ops, ((Op){.type = StoreVar32, .value.store_offset = offset, .value.size = DoubleWord, .value.arg_store = arg}));
                    consume();
                }

                if (!expect_type(SemiColon, "COMPILATION ERROR: Expected ';' after variable declaration")) return false;
              
            } break;
            // TODO: Only routines call for now
            case Identifier: {
                const char* routine_name = strdup(get_value().items);
                consume();
                if (!expect_type(LeftParen, "COMPILATION ERROR: Expected '(' after routine call")) return false;
                
                consume();
                if (get_type() == Identifier) {
                    int offset = shget(vars, get_value().items);
                    if (offset == -1) {
                        error("COMPILATION ERROR: Usage of undeclared variable");
                        return false;
                    } 

                    arrput(*ops, ((Op){.type = RoutineCall, .value.name = routine_name, .value.arg.type = Var, .value.arg.value.offset = offset}));
                    consume();
                } else if (get_type() == NumberLiteral) {
                    TODO("");
                }

                if (!expect_type(RightParen, "COMPILATION ERROR: Expected ')' after argument list")) return false;
                consume();
                if (!expect_type(SemiColon, "COMPILATION ERROR: Expected ';' after variable declaration")) return false;
            } break;
            case Eof: {
                shfree(vars);
                error("COMPILATION ERROR: Expected '}' after routine body");
                return false;
            }
            default: {
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
            break;
        }
        case StoreVar32: {
            printf("StoreVar32 ");
            printf("{ .store_offset = %zu, .size = %s, .arg = %d }", op.value.store_offset, size_to_str(op.value.size), op.value.arg_store);
            break;
        }
        case LoadVar32: {
            printf("LoadVar32 ");
            printf("{ .load_offset = %zu }", op.value.load_offset);
            break;
        }
        case RoutineCall: {
            printf("RoutineCall ");
            printf("{ .name = %s, .arg.value.offset = %zu }", op.value.name, op.value.arg.value.offset);
            break;
        }
        default: UNREACHABLE("");
    }
}

static void dump_ops(Op* ops) {
    size_t len = arrlen(ops);

    printf("Operations: \n");
    for (size_t i = 0; i < len; ++i) {
        dump_op(ops[i]);
        printf("\n");
    }
    printf("\n");
}

static bool generate_IA_x86_64(String_Builder* out, Op* ops) {
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
            case StoreVar32: sb_appendf(out, "    mov dword ptr [rsp - %zu], %d\n", op.value.store_offset, op.value.arg_store); break;
            case RoutineCall: {
                sb_appendf(out, "    mov rdi, qword ptr [rsp - %zu]\n", op.value.arg.value.offset);
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

static void print_usage(char** argv) {
    fprintf(stderr, "Usage: %s [OPTIONS] <inputs...> \n", argv[0]);
    fprintf(stderr, "OPTIONS: \n");
    fprintf(stderr, "    -o\n");
    fprintf(stderr, "        Output path\n");
}

#define WRONG_USAGE 2
#define FILE_NOT_FOUND 3
#define GEN_ERROR 4
#define FAILED_CMD 5

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv);
        exit(WRONG_USAGE);
    }

    int file_name = 1;

    String_Builder output_path = {0};
    if (strcmp(argv[1], "-o") == 0) {
        sb_appendf(&output_path, "%s", argv[2]);
        file_name = 3;
    }

    if (!init_lexer(argv[file_name])) {
        exit(FILE_NOT_FOUND);
    }

    init_compiler();

    Op* ops = NULL;
    generate_ops(&ops);
    dump_ops(ops);

    String_Builder result = {0};
    if (!generate_IA_x86_64(&result, ops)) exit(GEN_ERROR);
    arrfree(ops);

    String_Builder asm_file = {0};
    sb_appendf(&asm_file, "%s.asm", argv[file_name]);
    sb_append_null(&asm_file);
    FILE* assembly = fopen(asm_file.items, "w+");
    fprintf(assembly, "%s", result.items);
    fclose(assembly);

    if (output_path.count > 0) {
        Cmd compile = {0};
        nob_cc(&compile);
        nob_cc_output(&compile, output_path.items);
        nob_cc_inputs(&compile, asm_file.items);
        if (!cmd_run_sync_and_reset(&compile)) exit(FAILED_CMD);
    }

    sb_free(asm_file);
    sb_free(result);
    free_lexer();
    free_compiler();
    exit(0);
}
