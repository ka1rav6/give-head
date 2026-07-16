#include "lexer.h"
#include <iostream>

static const char* token_kind_name(Lexer::TokenKind kind) {
    using K = Lexer::TokenKind;
    switch (kind) {
        case K::IDENTIFIER:      return "IDENTIFIER";
        case K::KEYWORD:         return "KEYWORD";
        case K::INT_LITERAL:     return "INT_LITERAL";
        case K::FLOAT_LITERAL:   return "FLOAT_LITERAL";
        case K::STRING_LITERAL:  return "STRING_LITERAL";
        case K::CHAR_LITERAL:    return "CHAR_LITERAL";
        case K::HEX_LITERAL:     return "HEX_LITERAL";
        case K::OCTAL_LITERAL:   return "OCTAL_LITERAL";
        case K::LPAREN:          return "LPAREN";
        case K::RPAREN:          return "RPAREN";
        case K::LSQUARE:         return "LSQUARE";
        case K::RSQUARE:         return "RSQUARE";
        case K::LCURLY:          return "LCURLY";
        case K::RCURLY:          return "RCURLY";
        case K::SEMICOLON:       return "SEMICOLON";
        case K::COLON:           return "COLON";
        case K::COMMA:           return "COMMA";
        case K::DOT:             return "DOT";
        case K::TILDE:           return "TILDE";
        case K::QUESTION:        return "QUESTION";
        case K::PLUS:            return "PLUS";
        case K::MINUS:           return "MINUS";
        case K::STAR:            return "STAR";
        case K::SLASH:           return "SLASH";
        case K::PERCENT:         return "PERCENT";
        case K::AMPERSAND:       return "AMPERSAND";
        case K::PIPE:            return "PIPE";
        case K::CARET:           return "CARET";
        case K::BANG:            return "BANG";
        case K::EQUALS:          return "EQUALS";
        case K::LT:              return "LT";
        case K::GT:              return "GT";
        case K::PLUS_PLUS:       return "PLUS_PLUS";
        case K::MINUS_MINUS:     return "MINUS_MINUS";
        case K::ARROW:           return "ARROW";
        case K::EQ_EQ:           return "EQ_EQ";
        case K::BANG_EQ:         return "BANG_EQ";
        case K::LT_EQ:           return "LT_EQ";
        case K::GT_EQ:           return "GT_EQ";
        case K::AMP_AMP:         return "AMP_AMP";
        case K::PIPE_PIPE:       return "PIPE_PIPE";
        case K::LT_LT:           return "LT_LT";
        case K::GT_GT:           return "GT_GT";
        case K::AMP_EQ:          return "AMP_EQ";
        case K::PIPE_EQ:         return "PIPE_EQ";
        case K::CARET_EQ:        return "CARET_EQ";
        case K::PLUS_EQ:         return "PLUS_EQ";
        case K::MINUS_EQ:        return "MINUS_EQ";
        case K::STAR_EQ:         return "STAR_EQ";
        case K::SLASH_EQ:        return "SLASH_EQ";
        case K::PERCENT_EQ:      return "PERCENT_EQ";
        case K::ELLIPSIS:        return "ELLIPSIS";
        case K::PREPROCESSOR:    return "PREPROCESSOR";
        case K::LINE_COMMENT:    return "LINE_COMMENT";
        case K::BLOCK_COMMENT:   return "BLOCK_COMMENT";
        case K::NEWLINE:         return "NEWLINE";
        case K::UNKNOWN:         return "UNKNOWN";
    }
    return "UNKNOWN";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: give-head <file.c>" << std::endl;
        return 1;
    }

    std::string fileName = argv[1];
    std::vector<std::string> lines = Lexer::read_file(fileName);
    if (lines.empty()) {
        std::cerr << "Error: no lines read from " << fileName << std::endl;
        return 1;
    }

    for (size_t line_num = 0; line_num < lines.size(); line_num++) {
        std::vector<Lexer::Token> tokens = Lexer::tokenise_line(lines[line_num], line_num + 1);
        for (const auto& tok : tokens) {
            std::cout << "[Line " << tok.line_num
                      << ", Col " << tok.col_num
                      << "] " << token_kind_name(tok.kind)
                      << ": \"" << tok.raw << "\"" << std::endl;
        }
    }

    return 0;
}
