#include <assert.h>
#include <complex.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

// TODO: dev, rendere queste due funzioni meglio.
void unreachable() { assert(false && "Unreachable code"); }
void todo() { assert(false && "Not yet implemented"); }

// TODO: refactoring togliere None
typedef enum {
    None,
    Routine,
    Return,
    Identifier,
    SemiColon,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    Slash,
    Greater,
    Less,

    NumberLiteral,
    StringLiteral
} TokenType;

typedef struct {
    TokenType type;
    String_Builder *content;
} Token;

typedef struct {
    Token* vector;
    size_t size;
    size_t capacity;
} TokenVec;

#define INITIAL_CAPACITY 64

TokenVec* new_tokenvec() {
    TokenVec* vec = malloc(sizeof(TokenVec));

    vec->size = 0;
    vec->capacity = INITIAL_CAPACITY;
    vec->vector = malloc(sizeof(Token) * INITIAL_CAPACITY);
    assert(vec->vector != NULL);

    return vec;
}


void append_token(TokenVec* vec, Token token) {
    if (vec->size + 1 >= vec->capacity) {
        vec->capacity *= 2;
        vec->vector = realloc(vec->vector, sizeof(Token) * vec->capacity);
        assert(vec->vector != NULL);
    }

    vec->vector[vec->size] = token;
    vec->size += 1;
}

void print_tokenvec(TokenVec* vec) {
    for (size_t i = 0; i < vec->size; ++i) {
        TokenType type = vec->vector[i].type;
        char* string = vec->vector[i].content->items;

        switch (type) {
            case LeftParen: printf("("); break;
            case RightParen: printf(")"); break;
            case LeftBracket: printf("{"); break;
            case RightBracket: printf("}"); break;
            case SemiColon: printf(";"); break;
            case Greater: printf(">"); break;
            case Less: printf("<"); break;
            default: { printf("type: %d, %s", type, string); }
        }

        printf("\n");
    }
}


void free_tokenvec(TokenVec* vec) {
    for (size_t i = 0; i < vec->size; ++i) {
        if (vec->vector[i].content != NULL) sb_free(*vec->vector[i].content);
    }

    free(vec->vector);
    free(vec);
}

typedef struct { 
    char* key;
    TokenType value;
} HashMap;

HashMap* hm_keywords = NULL;

typedef struct {
    char* file_content;
    size_t position;
    size_t file_len;
} Lexer;

Lexer lexer = {0};

void init_lexer(char* source) {
    lexer.file_content = source;
    lexer.file_len = strlen(source);
    lexer.position = 0;
}

char* peek_ptr() {
    assert(lexer.position < lexer.file_len);
    return lexer.file_content + lexer.position;
}

char peek() {
    assert(lexer.position < lexer.file_len);
    return lexer.file_content[lexer.position];
}

char peek_prev() {
    assert(lexer.position - 1 >= 0);
    return lexer.file_content[lexer.position - 1];
}

int peek_next() {
    assert(lexer.position < lexer.file_len);
    if (lexer.position + 1 >= lexer.file_len) return -1;
    return lexer.file_content[lexer.position + 1];
}

char consume() {
    assert(lexer.position < lexer.file_len);
    return lexer.file_content[lexer.position++];
}

//         - last character
// h e l l o
// ^ first character
void string_token(Token* tok) {
    tok->type = StringLiteral;
    // TODO error, manager unterminated string
    while (peek() != '"') sb_appendf(tok->content, "%c", consume());
    sb_append_null(tok->content);
    consume();
}

void identifier(Token* tok) {
    sb_appendf(tok->content, "%c", peek_prev());
    while (isalnum(peek())) sb_appendf(tok->content, "%c", consume());
    sb_append_null(tok->content);

    int result = shget(hm_keywords, tok->content->items);
    if (result != None) tok->type = result;
    else tok->type = Identifier;
}

void number(Token* tok) {
    tok->type = NumberLiteral;
    sb_appendf(tok->content, "%c", peek_prev());
    while (isdigit(peek())) sb_appendf(tok->content, "%c", consume());
    sb_append_null(tok->content);
}

Token* next_token() {
    Token* token = malloc(sizeof(Token));
    token->type = None;
    token->content = malloc(sizeof(String_Builder));
    memset(token->content, 0, sizeof(String_Builder));

    switch (consume()) {
        case '(': token->type = LeftParen; break;
        case ')': token->type = RightParen; break;
        case '{': token->type = LeftBracket; break;
        case '}': token->type = RightBracket; break;
        case ';': token->type = SemiColon; break;
        case '>': token->type = Greater; break;
        case '<': token->type = Less; break;
        case '"': string_token(token); break;
        case '/': {
                      if (peek() == '/') while (peek() != '\n') consume();
                      else token->type = Slash;
                      break;
                  }
        default: {
                     if (isalpha(peek_prev())) {
                         identifier(token); 
                     } else if (isdigit(peek_prev())) {
                         number(token); 
                     } else {
                         unreachable();
                     }
                 }
    }

    return token;
}

TokenVec* parse() {
    TokenVec* vec = new_tokenvec();

    while (lexer.position < lexer.file_len) {
        if (isspace(peek())) {
            consume();
            continue;            
        }

        // TODO refactoring: Next token deve sempre ritornare il prossimo token
        Token* token = next_token();
        if (token->type != None) append_token(vec, *token);
        else free(token);
    }

    return vec;
}

char* read_file_to_string(char* file_name) {
    FILE* source = fopen(file_name, "r");
    if (source == NULL) return NULL; 

    int len = 0;
    while (fgetc(source) != EOF) len += 1; 
    rewind(source);

    char* string = malloc(sizeof(char) * len + 1);
    if (string == NULL) return NULL;

    for (int i = 0; i < len; ++i) string[i] = fgetc(source); 
    string[len] = '\0';

    fclose(source);
    return string;
}

void init_hm_keywords() {
    shput(hm_keywords, "rt", Routine);
    shput(hm_keywords, "ret", Return);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: au INPUT\n");
        exit(2);
    }

    init_hm_keywords();

    char* content = read_file_to_string(argv[1]);
    if (content == NULL) exit(3);

    init_lexer(content);
    TokenVec* vec = parse();

    print_tokenvec(vec);

    free_tokenvec(vec);
    free(content);
    shfree(hm_keywords);
    exit(0);
}
