#ifndef LEXER_HEADER
#define LEXER_HEADER

#include <stdbool.h>
#include "token.h"

#define NOB_STRIP_PREFIX
#include "../nob.h"

typedef struct { 
    char* key;
    TokenType value;
} HashMap;

typedef struct {
    const char* input_stream; // File name

    String_Builder file_content; // File content
    size_t position; // Position in content

    TokenType token_type;
    String_Builder token_value;

    size_t line_number;
    size_t line_offset_start;
    size_t line_offset_end;
} Lexer;

void error(const char* msg);
Token get_token();
bool next_token();
bool init_lexer(const char* input_stream);
void free_lexer();

TokenType get_type();
String_Builder get_value();

#endif
