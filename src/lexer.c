#include "lexer.h"
#include "token.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "../nob.h"

static Lexer lexer = {0};

TokenType get_type() {
    return lexer.token_type;
}

String_Builder get_value() {
    return lexer.token_value;
}

void error(const char* msg) {
    fprintf(stderr, "%s:%zu:%zu: %s\n", lexer.input_stream, lexer.line_number, lexer.line_offset_start, msg);
}

static bool is_eof() {
    return lexer.position >= lexer.file_content.count;
}

static char peek() {
    return lexer.file_content.items[lexer.position];
}

static char peek_next() {
    assert(lexer.position + 1 < lexer.file_content.count);
    return lexer.file_content.items[lexer.position + 1];
}

static char peek_prev() {
    assert(lexer.position - 1 >= 0);
    return lexer.file_content.items[lexer.position - 1];
}

static char consume() {
    assert(!is_eof());
    lexer.line_offset_end++;
    return lexer.file_content.items[lexer.position++];
}

//         - last character
// h e l l o
// ^ first character
static void parse_string() {
    lexer.token_type = StringLiteral;
    lexer.line_offset_start = lexer.line_offset_end - 1;
    while (peek() != '"' && !is_eof()) sb_appendf(&lexer.token_value, "%c", consume());

    if (is_eof()) {
        error("PARSE ERROR: Unterminated string");
        lexer.token_type = ParseError;
    }

    consume();
    sb_append_null(&lexer.token_value);
}

static TokenType keyword_id(String_Builder* id) {
    TokenType result = Identifier;

    if (strcmp(id->items, "rt") == 0) {
        result = Routine; 
    } else if (strcmp(id->items, "ret") == 0) {
        result = Return;
    }

    return result;
}

static void parse_identifier() {
    sb_appendf(&lexer.token_value, "%c", peek_prev());
    while (isalnum(peek())) sb_appendf(&lexer.token_value, "%c", consume());
    sb_append_null(&lexer.token_value);
    lexer.token_type = keyword_id(&lexer.token_value);
}

static void parse_number() {
    lexer.token_type = NumberLiteral;
    sb_appendf(&lexer.token_value, "%c", peek_prev());
    while (isdigit(peek())) sb_appendf(&lexer.token_value, "%c", consume());

    if (peek() == '.') {
        sb_appendf(&lexer.token_value, ".");
        consume();
        lexer.token_type = DoubleLiteral;
        while (isdigit(peek())) sb_appendf(&lexer.token_value, "%c", consume());
    }

    sb_append_null(&lexer.token_value);
}

Token get_token() {
    Token token = {.type = lexer.token_type, .value = lexer.token_value};
    return token;
}

static void reset_token() {
    sb_free(lexer.token_value);
    String_Builder value = {0};
    lexer.token_value = value;
    lexer.token_type = ParseError;
}

bool next_token() {
    reset_token();
    lexer.line_offset_start = lexer.line_offset_end;

    while (isspace(peek())) {
        char space = consume(); 
        if (space == '\n') {
            ++lexer.line_number;
            lexer.line_offset_end = 1;
        }
    }

    if (is_eof()) return false;

    // TODO: Support multiline comments
    if (peek() == '/' && peek_next() == '/') {
        while (peek() != '\n') consume();
        return next_token();
    }

    bool result = true;
    switch (consume()) {
        case '(': lexer.token_type = LeftParen; break;
        case ')': lexer.token_type = RightParen; break;
        case '{': lexer.token_type = LeftBracket; break;
        case '}': lexer.token_type = RightBracket; break;
        case ';': lexer.token_type = SemiColon; break;
        case '>': lexer.token_type = Greater; break;
        case '=': lexer.token_type = Equal; break;
        case '<': lexer.token_type = Less; break;
        case '/': lexer.token_type = Slash; break;
        case '"': parse_string(); break;
        default: {
                     if (isalpha(peek_prev())) {
                         parse_identifier(); 
                     } else if (isdigit(peek_prev())) {
                         parse_number(); 
                     } else {
                         UNREACHABLE("Weird");
                     }
                 }
    }

    return result;
}

bool init_lexer(const char* input_stream) {
    String_Builder source = {0};
    if (!read_entire_file(input_stream, &source)) return false;

    lexer.input_stream = input_stream;

    lexer.file_content = source;
    lexer.position = 0;

    String_Builder value = {0};
    lexer.token_value = value;
    lexer.token_type = ParseError;

    lexer.line_number = 1;
    lexer.line_offset_end = 1;
    lexer.line_offset_start = lexer.line_offset_end;

    return true;
}

void free_lexer() {
    sb_free(lexer.file_content);
    sb_free(lexer.token_value);
}
