#include "token.h"
#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOB_STRIP_PREFIX
#include "../nob.h"

#define FAILED_CMD 5
#define GEN_ERROR 4
#define FILE_NOT_FOUND 3
#define WRONG_USAGE 2

typedef struct {} Compiler;
// static Compiler comp = {};
static void init_compiler() {}
static void free_compiler() {}

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

bool generate_IA_x86_64(String_Builder* out) {
    sb_appendf(out, ".intel_syntax noprefix\n");
    sb_appendf(out, ".text\n");

    expect_and_consume_ret(Routine, "COMPILATION ERROR: Expected Routine");

    expect_ret(Identifier, "COMPILATION ERROR: Expected routine name");
    Token main = get_token();
    if (strcmp(main.value.items, "main") != 0) {
        error("COMPILATION ERROR: Expected the Function to be named 'main'");
        return false;
    }
    consume();

    expect_and_consume_ret(LeftParen, "COMPILATION ERROR: Expected '(' after routine name");
    expect_and_consume_ret(RightParen, "COMPILATION ERROR: Expected ')'");
    expect_and_consume_ret(LeftBracket, "COMPILATION ERROR: Expected '{'");

    sb_appendf(out, ".globl main\n\n");
    sb_appendf(out, "main:\n");
    sb_appendf(out, "    push rbp\n");

    while (get_token().type != Eof) {
        if (get_token().type == Identifier) {
            if (strcmp("putchar", get_token().value.items) == 0) {
                consume();
                expect_and_consume_ret(LeftParen, "COMPILATION ERROR: Expected '('");

                expect_ret(NumberLiteral, "COMPILATION ERROR: Expected integer");
                int num = atoi(get_token().value.items);
                consume();

                expect_and_consume_ret(RightParen, "COMPILATION ERROR: Expected ')'");
                expect_and_consume_ret(SemiColon, "COMPILATION ERROR: Expected ';'");

                sb_appendf(out, "    mov edi, %d\n", num);
                sb_appendf(out, "    call putchar\n");
            } else {
                error("COMPILATION ERROR: Unknown identifier");
                return false;
            }
        } else if (get_token().type == RightBracket) {
            consume();
        } else {
            printf("%s\n", display_type(get_token().type));
            error("COMPILATION ERROR: Unsupported token type");
            break;
        }
    }

    sb_appendf(out, "    pop rbp\n");
    sb_appendf(out, "    mov rax, 0\n");
    sb_appendf(out, "    ret\n");

    sb_append_null(out);
    return true;
}

static void print_usage(char** argv) {
    fprintf(stderr, "Usage: %s [OPTIONS] <inputs...> \n", argv[0]);
    fprintf(stderr, "OPTIONS: \n");
    fprintf(stderr, "    -o\n");
    fprintf(stderr, "        Output path\n");
}

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

    String_Builder result = {0};
    if (!generate_IA_x86_64(&result)) exit(GEN_ERROR);

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
    return 0;
}
