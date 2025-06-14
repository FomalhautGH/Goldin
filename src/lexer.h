#ifndef LEXER_HEADER
#define LEXER_HEADER

#include "token.h"

typedef struct { 
    char* key;
    TokenType value;
} HashMap;

typedef struct {
    String_Builder *file_content;
    size_t position;
} Lexer;

void init_hm_keywords();
void free_hm_keywords();
void init_lexer(String_Builder* source); 
char peek(); 
char peek_prev(); 
int peek_next(); 
char consume(); 
void string_token(Token* tok); 
void identifier(Token* tok); 
void number(Token* tok); 
Token* next_token(); 
TokenVec* parse(); 

#endif
