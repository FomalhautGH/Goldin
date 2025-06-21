#include "token.h"
#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GEN_ERROR 4
#define FILE_NOT_FOUND 3
#define WRONG_USAGE 2

typedef struct {
    TokenVec* tokens;
    size_t position;
    
} Compiler;

static Compiler comp = {0};
static void init_compiler(TokenVec* vec) {
    comp.tokens = vec;
    comp.position = 0;
}

static Token* consume() {
    return tokenvec_get(comp.tokens, comp.position++);
}

static Token* peek() {
    return tokenvec_get(comp.tokens, comp.position);
}

static bool expect_type(TokenType type) {
    return peek()->type == type;
}

// TODO: Indicate error line
static void error(const char* msg) {
    fprintf(stderr, "Syntax error: %s\n", msg);
}

static Token* consume_if(TokenType type, const char* msg) {
    if (expect_type(type)) return consume();
    error(msg);    
    return NULL;
}

bool generate_IA_x86_64(String_Builder* out) {
    sb_appendf(out, ".intel_syntax noprefix\n");
    sb_appendf(out, ".text\n");

    if (!consume_if(Routine, "Expected Routine")) return false;

    Token* rt_name = consume_if(Identifier, "Expected Function Name");
    if (rt_name == NULL) return false;

    if (strcmp(rt_name->content->items, "main") != 0) {
        error("Expected the Function to be named 'main'");
        return false;
    }

    if (!consume_if(LeftParen, "Expected '(' after function name")) return false;
    if (!consume_if(RightParen, "Expected ')'")) return false;
    if (!consume_if(LeftBracket, "Expected '{'")) return false;

    sb_appendf(out, ".globl main\n\n");
    sb_appendf(out, "main:\n");
    sb_appendf(out, "   push rbp\n");

    while (peek()->type != RightBracket) {
        Token* tok = consume(); 
        if (tok->type == Identifier) {
            if (strcmp("putchar", tok->content->items) == 0) {
                consume_if(LeftParen, "Expected '('");
                Token* param = consume_if(NumberLiteral, "Expected integer");
                consume_if(RightParen, "Expected ')'");
                consume_if(SemiColon, "Expected ';'");

                int int_p = atoi(param->content->items);
                sb_appendf(out, "   mov edi, %d\n", int_p);
                sb_appendf(out, "   call putchar\n");
            } else {
                error("Unknown identifier");
                return false;
            }
        } else {
            error("Unknown token");
            return false;
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

    String_Builder content = {0};
    if (!read_entire_file(argv[1], &content)) exit(FILE_NOT_FOUND);

    init_lexer(&content);

    TokenVec* vec = parse();
    print_tokenvec(vec);

    init_compiler(vec);
    String_Builder result = {0};
    if (!generate_IA_x86_64(&result)) exit(GEN_ERROR);

    // printf("\nGenerated Assembly: \n%s\n", result.items);

    FILE* assembly = fopen("./out.s", "w+");
    fprintf(assembly, "%s", result.items);

    sb_free(result);
    sb_free(content);
    free_tokenvec(vec);
    free_lexer();
    exit(0);
}
