#include "token.h"

const char* display_type(TokenType type) {
    switch (type) {
        case Eof: return "Eof";
        case ParseError: return "ParseError";
        case Routine: return "Routine";
        case Return: return "Return";
        case Identifier: return "Identifier";
        case SemiColon: return "SemiColon";
        case LeftParen: return "LeftParen";
        case RightParen: return "RightParen";
        case LeftBracket: return "LeftBracket";
        case RightBracket: return "RightBracket";
        case Slash: return "Slash";
        case Equal: return "Equal";
        case Greater: return "Greater";
        case Less: return "Less";
        case NumberLiteral: return "NumberLiteral";
        case DoubleLiteral: return "DoubleLiteral";
        case StringLiteral: return "StringLiteral";
        default: UNREACHABLE("Unknown type to display");
    }
}
