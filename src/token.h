#ifndef TOKEN_HEADER
#define TOKEN_HEADER

#define NOB_STRIP_PREFIX
#include "nob.h"

typedef enum {
    Eof,
    ParseError,
    Routine,
    Return,
    Comma,
    SemiColon,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    While,

    VarTypei8,
    VarTypei16,
    VarTypei32,
    VarTypei64,
    VarTypeu8,
    VarTypeu16,
    VarTypeu32,
    VarTypeu64,
    VarTypef32,
    VarTypef64,

    Equal,
    Greater,
    Less,
    Plus,
    Minus,
    Star,
    Slash,

    Identifier,
    IntLiteral,
    RealLiteral,
    StringLiteral
} TokenType;

typedef struct {
    TokenType type;
    String_Builder value;
} Token;

const char* display_type(TokenType type);

#endif
