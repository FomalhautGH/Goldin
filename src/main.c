#include "token.h"
#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOB_STRIP_PREFIX
#include "../nob.h"

#define GEN_ERROR 4
#define FILE_NOT_FOUND 3
#define WRONG_USAGE 2

typedef struct {} Compiler;
static Compiler comp = {};
static void init_compiler() { }

static bool expect_type(TokenType type) {
    return get_token().type == type;
}

static bool consume_expect(TokenType type, const char* msg) {
    next_token();

    if (!expect_type(type)) {
        error(msg);
        return false;
    }

    return true;
}

#define consume_exret(type, msg) \
    do { \
        if (!consume_expect(type, msg)) return false; \
    } while (0) 

// TODO: Indicate error line
// static void error(const char* msg) {
//     fprintf(stderr, "Syntax error: %s\n", msg);
// }

bool generate_IA_x86_64(String_Builder* out) {
    sb_appendf(out, ".intel_syntax noprefix\n");
    sb_appendf(out, ".text\n");

    consume_exret(Routine, "COMPILATION ERROR: Expected Routine");
    consume_exret(Identifier, "COMPILATION ERROR: Expected routine name");

    Token name = get_token();
    if (strcmp(name.value.items, "main") != 0) {
        error("COMPILATION ERROR: Expected the Function to be named 'main'");
        return false;
    }

    consume_exret(LeftParen, "COMPILATION ERROR: Expected '(' after routine name");
    consume_exret(RightParen, "COMPILATION ERROR: Expected ')'");
    consume_exret(LeftBracket, "COMPILATION ERROR: Expected '{'");

    sb_appendf(out, ".globl main\n\n");
    sb_appendf(out, "main:\n");
    sb_appendf(out, "   push rbp\n");

    for (;;) {
        next_token();
        if (get_token().type == Identifier) {
            Token id = get_token();
            if (strcmp("putchar", id.value.items) == 0) {
                consume_exret(LeftParen, "COMPILATION ERROR: Expected '('");
                consume_exret(NumberLiteral, "COMPILATION ERROR: Expected integer");
                int num = atoi(get_token().value.items);
                consume_exret(RightParen, "COMPILATION ERROR: Expected ')'");
                consume_exret(SemiColon, "COMPILATION ERROR: Expected ';'");

                sb_appendf(out, "   mov edi, %d\n", num);
                sb_appendf(out, "   call putchar\n");
            } else {
                error("Unknown identifier");
                return false;
            }
        } else if (get_token().type == RightBracket) {
            break;
        }
    }

    sb_appendf(out, "   pop rbp\n");
    sb_appendf(out, "   mov rax, 0\n");
    sb_appendf(out, "   ret\n");

    sb_append_null(out);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: au INPUT\n");
        exit(WRONG_USAGE);
    }

    if (!init_lexer(argv[1])) {
        exit(FILE_NOT_FOUND);
    }

    init_compiler();

    String_Builder result = {0};
    if (!generate_IA_x86_64(&result)) exit(GEN_ERROR);

    // printf("\nGenerated Assembly: \n%s\n", result.items);

    FILE* assembly = fopen("./out.s", "w+");
    fprintf(assembly, "%s", result.items);

    sb_free(result);
    free_lexer();

    fclose(assembly);
    exit(0);
}
