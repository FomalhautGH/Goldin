#include "lexer.h"
#include "token.h"
#include <string.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

static Lexer lexer = {0};

static bool is_eof() {
    return lexer.position >= lexer.file_content.count;
}

static char peek() {
    assert(!is_eof());
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

static char consume_inside() {
    if (peek() == '\n') ++lexer.line_number_end;
    ++lexer.line_offset_end;
    return lexer.file_content.items[lexer.position++];
}

static char consume() {
    if (peek() == '\n') {
        ++lexer.line_number_end;
        lexer.line_offset_start = 1;
        lexer.line_offset_end = 0;
    }

    ++lexer.line_offset_end;
    return lexer.file_content.items[lexer.position++];
}

static void keyword_id() {
    lexer.token_type = Identifier;
    char* id = lexer.token_value.items;

    if (strcmp(id, "rt") == 0) {
        lexer.token_type = Routine;
    } else if (strcmp(id, "ret") == 0) {
        lexer.token_type = Return;
    } else if (strcmp(id, "if") == 0) {
        lexer.token_type = If;
    } else if (strcmp(id, "else") == 0) {
        lexer.token_type = Else;
    } else if (strcmp(id, "while") == 0) {
        lexer.token_type = While;
    } else if (strcmp(id, "i8") == 0) {
        lexer.token_type = VarTypei8;
    } else if (strcmp(id, "i16") == 0) {
        lexer.token_type = VarTypei16;
    } else if (strcmp(id, "i32") == 0) {
        lexer.token_type = VarTypei32;
    } else if (strcmp(id, "i64") == 0) {
        lexer.token_type = VarTypei64;
    } else if (strcmp(id, "u8") == 0) {
        lexer.token_type = VarTypeu8;
    } else if (strcmp(id, "u16") == 0) {
        lexer.token_type = VarTypeu16;
    } else if (strcmp(id, "u32") == 0) {
        lexer.token_type = VarTypeu32;
    } else if (strcmp(id, "u64") == 0) {
        lexer.token_type = VarTypeu64;
    } else if (strcmp(id, "f32") == 0) {
        lexer.token_type = VarTypef32;
    } else if (strcmp(id, "f64") == 0) {
        lexer.token_type = VarTypef64;
    }
}

static void push_char(char jar) {
    sb_appendf(&lexer.token_value, "%c", jar);
}

static void parse_string() {
    lexer.token_type = StringLiteral;
    lexer.line_number_start = lexer.line_number_end;
    lexer.line_offset_start = lexer.line_offset_end - 1;

    while (!is_eof() && peek() != '"') push_char(consume_inside());

    if (is_eof()) {
        error_msg("PARSE ERROR: Unterminated string");
        lexer.token_type = ParseError;
    } else {
        consume();
    }
}

static void parse_identifier() {
    lexer.line_number_start = lexer.line_number_end;
    lexer.line_offset_start = lexer.line_offset_end - 1;
    push_char(peek_prev());
    while (isalnum(peek())) push_char(consume());
    keyword_id();
}

static void parse_number() {
    lexer.token_type = IntLiteral;
    push_char(peek_prev());
    while (isdigit(peek())) push_char(consume());

    if (peek() == '.') {
        push_char('.');
        consume();
        lexer.token_type = RealLiteral;
        while (isdigit(peek())) push_char(consume());
    }
}

static void reset_previus_token() {
    sb_free(lexer.token_value);
    lexer.token_value = (String_Builder) {0};
    lexer.token_type = ParseError;
    lexer.line_offset_start = lexer.line_offset_end; 
    lexer.line_number_start = lexer.line_number_end;
}

void error_msg(const char* msg) {
    fprintf(stderr, "%s:%zu:%zu: %s\n", lexer.input_stream, lexer.line_number_start, lexer.line_offset_start, msg);
}

void error_expected(TokenType expected, TokenType got) {
    String_Builder msg = {0};
    sb_appendf(&msg, "Expected '%s' but instead got '%s'", display_type(expected), display_type(got));
    sb_append_null(&msg);
    error_msg(msg.items);
    sb_free(msg);
}

Token get_token() {
    return (Token){.type = lexer.token_type, .value = lexer.token_value};
}

TokenType get_type() {
    return lexer.token_type;
}

String_Builder get_value() {
    return lexer.token_value;
}

bool next_token() {
    reset_previus_token();

    while (!is_eof() && isspace(peek())) consume(); 

    if (is_eof()) {
        lexer.token_type = Eof;
        return false;
    }

    // TODO: Support multiline comments
    if (peek() == '/' && peek_next() == '/') {
        while (peek() != '\n') consume();
        return next_token();
    }

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
        case '+': lexer.token_type = Plus; break;
        case '-': lexer.token_type = Minus; break;
        case ',': lexer.token_type = Comma; break;
        case '*': lexer.token_type = Star; break;
        case '"': parse_string(); break;
        default: {
            if (isalpha(peek_prev())) parse_identifier(); 
            else if (isdigit(peek_prev())) parse_number(); 
            else UNREACHABLE("Unsupported character");
        }
    }

    return true;
}

bool init_lexer(const char* input_stream) {
    String_Builder source = {0};
    if (!read_entire_file(input_stream, &source)) return false;

    lexer.input_stream = input_stream;

    lexer.file_content = source;
    lexer.position = 0;

    lexer.token_value = (String_Builder) {0};
    lexer.token_type = ParseError;

    lexer.line_number_end = 1;
    lexer.line_number_start = lexer.line_offset_end;
    lexer.line_offset_end = 1;
    lexer.line_offset_start = lexer.line_offset_end;

    return true;
}

void free_lexer() {
    sb_free(lexer.file_content);
    sb_free(lexer.token_value);
}
