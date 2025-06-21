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

TokenVec* parse(); 
void init_lexer(String_Builder* source); 
void free_lexer();

#endif
