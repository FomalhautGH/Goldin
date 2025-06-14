#ifndef TOKEN_HEADER
#define TOKEN_HEADER

#define NOB_STRIP_PREFIX
#include "../nob.h"

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

TokenVec* new_tokenvec(); 
void append_token(TokenVec* vec, Token token); 
void print_tokenvec(TokenVec* vec); 
void free_tokenvec(TokenVec* vec); 

#endif
