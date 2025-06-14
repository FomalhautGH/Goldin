#include "lexer.h"

#define NOB_IMPLEMENTATION
#include "../nob.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

HashMap* hm_keywords = NULL;
Lexer lexer = {0};

void init_hm_keywords() {
    shput(hm_keywords, "rt", Routine);
    shput(hm_keywords, "ret", Return);
}

void free_hm_keywords() {
    shfree(hm_keywords);
}

void init_lexer(String_Builder* source) {
    lexer.file_content = source;
    lexer.position = 0;
}

char peek() {
    assert(lexer.position < lexer.file_content->count);
    return lexer.file_content->items[lexer.position];
}

char peek_prev() {
    assert(lexer.position - 1 >= 0);
    return lexer.file_content->items[lexer.position - 1];
}

int peek_next() {
    assert(lexer.position < lexer.file_content->count);
    if (lexer.position + 1 >= lexer.file_content->count) return -1;
    return lexer.file_content->items[lexer.position + 1];
}

char consume() {
    assert(lexer.position < lexer.file_content->count);
    return lexer.file_content->items[lexer.position++];
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
    Token* token = malloc(sizeof(Token)); // <-- Memory leak misterioso
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
                         UNREACHABLE("");
                     }
                 }
    }

    return token;
}

TokenVec* parse() {
    TokenVec* vec = new_tokenvec();

    while (lexer.position < lexer.file_content->count) {
        if (isspace(peek())) {
            consume();
            continue;            
        }

        // TODO refactoring: Next token deve sempre ritornare il prossimo token
        Token* token = next_token();
        if (token->type != None) append_token(vec, *token);
        else {
            free(token->content);
            free(token);
        }
    }

    return vec;
}
