#include "token.h"

const char* display_type(TokenType type) {
    switch (type) {
        case Eof: return "Eof";
        case ParseError: return "ParseError";
        case Routine: return "Routine";
        case Return: return "Return";
        case Identifier: return "Identifier";
        case SemiColon: return ";";
        case LeftParen: return "(";
        case RightParen: return ")";
        case LeftBracket: return "{";
        case RightBracket: return "}";
        case While: return "While";
        case Comma: return ",";
        case Slash: return "/";
        case Equal: return "=";
        case Greater: return ">";
        case Less: return "<";
        case Plus: return "+";
        case Minus: return "-";
        case Star: return "*";
        case VarTypei8: return "VarTypei8";
        case VarTypei16: return "VarTypei16";
        case VarTypei32: return "VarTypei32";
        case VarTypei64: return "VarTypei64";
        case VarTypeu8: return "VarTypeu8";
        case VarTypeu16: return "VarTypeu16";
        case VarTypeu32: return "VarTypeu32";
        case VarTypeu64: return "VarTypeu64";
        case VarTypef32: return "VarTypef32";
        case VarTypef64: return "VarTypef64";
        case IntLiteral: return "IntLiteral";
        case RealLiteral: return "RealLiteral";
        case StringLiteral: return "StringLiteral";
    }
}
