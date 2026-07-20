#include "lexer.h"
#include "keywords.h"
#include "logx_linux.h"
#include <iostream>
#include <fstream>
#include <cctype>

namespace Lexer{

std::vector<std::string> read_file(const std::string& fileName) {
    LOGX_DEBUG << "opening file: " << fileName;
    std::vector<std::string> lines;
    std::ifstream file(fileName);
    if (!file.is_open()) {
        LOGX_ERROR << "could not open file: " << fileName;
        std::cerr << "Error: could not open file: " << fileName << std::endl;
        return lines;
    }
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    LOGX_DEBUG << "read " << lines.size() << " lines from " << fileName;

    // Splice backslash-newline line continuations (C translation phase 2)
    // into a single logical line before tokenizing, e.g.:
    //   #define container_of(ptr, type, member) \
    //       ((type*)((char*)(ptr) - offsetof(type, member)))
    // Without this, the trailing '\' lexed as an UNKNOWN token and the
    // rest of the macro definition was truncated. Continuation lines are
    // merged forward and left as empty placeholders so every other
    // line's number stays accurate for diagnostics.
    for (size_t idx = 0; idx < lines.size(); idx++) {
        size_t end = lines[idx].find_last_not_of(" \t\r");
        size_t next = idx + 1;
        while (end != std::string::npos && lines[idx][end] == '\\' && next < lines.size()) {
            lines[idx] = lines[idx].substr(0, end) + " " + lines[next];
            lines[next].clear();
            next++;
            end = lines[idx].find_last_not_of(" \t\r");
        }
    }

    return lines;
}

static void skip_string(const std::string& line, size_t* i, char delim) {
    while (*i < line.size() && line[*i] != delim) {
        if (line[*i] == '\\')
            (*i)++;
        (*i)++;
    }
    if (*i < line.size())
        (*i)++;
}

static Token make_token(TokenKind kind, const std::string& line, size_t start, size_t end, size_t line_num) {
    return Token{kind, line.substr(start, end - start), start + 1, line_num};
}

std::vector<Token> tokenise_line(const std::string& line, size_t line_num) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t len = line.size();
    LOGX_DEBUG << "tokenising line " << line_num << ": \"" << line << "\"";

    while (i < len) {
        // skip whitespace (but not newlines)
        if (line[i] == ' ' || line[i] == '\t' || line[i] == '\r') {
            i++;
            continue;
        }

        size_t start = i;

        // ---- preprocessor directive ----
        if (line[i] == '#') {
            i++;
            while (i < len && line[i] != '\n')
                i++;
            LOGX_DEBUG << "  [line " << line_num << "] PREPROCESSOR: " << line.substr(start, i - start);
            tokens.push_back(make_token(TokenKind::PREPROCESSOR, line, start, i, line_num));
            continue;
        }

        // ---- single line comment ----
        if (line[i] == '/' && i + 1 < len && line[i + 1] == '/') {
            i = len;
            LOGX_DEBUG << "  [line " << line_num << "] LINE_COMMENT";
            tokens.push_back(make_token(TokenKind::LINE_COMMENT, line, start, i, line_num));
            continue;
        }

        // ---- block comment (single line only) ----
        if (line[i] == '/' && i + 1 < len && line[i + 1] == '*') {
            i += 2;
            while (i < len - 1 && !(line[i] == '*' && line[i + 1] == '/'))
                i++;
            if (i < len - 1)
                i += 2;
            LOGX_DEBUG << "  [line " << line_num << "] BLOCK_COMMENT";
            tokens.push_back(make_token(TokenKind::BLOCK_COMMENT, line, start, i, line_num));
            continue;
        }

        // ---- string literal ----
        if (line[i] == '"') {
            i++;
            skip_string(line, &i, '"');
            LOGX_DEBUG << "  [line " << line_num << "] STRING_LITERAL: " << line.substr(start, i - start);
            tokens.push_back(make_token(TokenKind::STRING_LITERAL, line, start, i, line_num));
            continue;
        }

        // ---- char literal ----
        if (line[i] == '\'') {
            i++;
            skip_string(line, &i, '\'');
            LOGX_DEBUG << "  [line " << line_num << "] CHAR_LITERAL: " << line.substr(start, i - start);
            tokens.push_back(make_token(TokenKind::CHAR_LITERAL, line, start, i, line_num));
            continue;
        }

        // ---- numbers (hex, octal, float, int) ----
        if (std::isdigit(line[i]) || (line[i] == '.' && i + 1 < len && std::isdigit(line[i + 1]))) {
            TokenKind kind = TokenKind::INT_LITERAL;

            if (line[i] == '0' && i + 1 < len && (line[i + 1] == 'x' || line[i + 1] == 'X')) {
                kind = TokenKind::HEX_LITERAL;
                i += 2;
                while (i < len && std::isxdigit(line[i]))
                    i++;
            } else if (line[i] == '0' && i + 1 < len && std::isdigit(line[i + 1])) {
                kind = TokenKind::OCTAL_LITERAL;
                while (i < len && std::isdigit(line[i]))
                    i++;
            } else {
                while (i < len && std::isdigit(line[i]))
                    i++;
                if (i < len && line[i] == '.') {
                    kind = TokenKind::FLOAT_LITERAL;
                    i++;
                    while (i < len && std::isdigit(line[i]))
                        i++;
                }
                if (i < len && (line[i] == 'e' || line[i] == 'E')) {
                    kind = TokenKind::FLOAT_LITERAL;
                    i++;
                    if (i < len && (line[i] == '+' || line[i] == '-'))
                        i++;
                    while (i < len && std::isdigit(line[i]))
                        i++;
                }
            }

            // consume type suffixes (u, l, ll, f, etc)
            while (i < len && (line[i] == 'u' || line[i] == 'U' ||
                               line[i] == 'l' || line[i] == 'L' ||
                               line[i] == 'f' || line[i] == 'F'))
                i++;

            tokens.push_back(make_token(kind, line, start, i, line_num));
            LOGX_DEBUG << "  [line " << line_num << "] NUMBER: " << line.substr(start, i - start);
            continue;
        }

        // ---- identifiers / keywords ----
        if (std::isalpha(line[i]) || line[i] == '_') {
            while (i < len && (std::isalnum(line[i]) || line[i] == '_'))
                i++;

            std::string_view word(line.c_str() + start, i - start);
            TokenKind kind = is_keyword(word) ? TokenKind::KEYWORD : TokenKind::IDENTIFIER;
            LOGX_DEBUG << "  [line " << line_num << "] " << (kind == TokenKind::KEYWORD ? "KEYWORD" : "IDENTIFIER") << ": " << word;
            tokens.push_back(make_token(kind, line, start, i, line_num));
            continue;
        }

        // ---- two/three character operators ----
        if (line[i] == '-' && i + 1 < len && line[i + 1] == '>') {
            i += 2;
            LOGX_DEBUG << "  [line " << line_num << "] ARROW";
            tokens.push_back(make_token(TokenKind::ARROW, line, start, i, line_num));
            continue;
        }
        if (line[i] == '+' && i + 1 < len && line[i + 1] == '+') {
            i += 2;
            tokens.push_back(make_token(TokenKind::PLUS_PLUS, line, start, i, line_num));
            continue;
        }
        if (line[i] == '-' && i + 1 < len && line[i + 1] == '-') {
            i += 2;
            tokens.push_back(make_token(TokenKind::MINUS_MINUS, line, start, i, line_num));
            continue;
        }
        if (line[i] == '=' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::EQ_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '!' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::BANG_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '<' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::LT_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '>' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::GT_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '&' && i + 1 < len && line[i + 1] == '&') {
            i += 2;
            tokens.push_back(make_token(TokenKind::AMP_AMP, line, start, i, line_num));
            continue;
        }
        if (line[i] == '|' && i + 1 < len && line[i + 1] == '|') {
            i += 2;
            tokens.push_back(make_token(TokenKind::PIPE_PIPE, line, start, i, line_num));
            continue;
        }
        if (line[i] == '<' && i + 1 < len && line[i + 1] == '<') {
            i += 2;
            tokens.push_back(make_token(TokenKind::LT_LT, line, start, i, line_num));
            continue;
        }
        if (line[i] == '>' && i + 1 < len && line[i + 1] == '>') {
            i += 2;
            tokens.push_back(make_token(TokenKind::GT_GT, line, start, i, line_num));
            continue;
        }
        if (line[i] == '&' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::AMP_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '|' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::PIPE_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '^' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::CARET_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '+' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::PLUS_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '-' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::MINUS_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '*' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::STAR_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '/' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::SLASH_EQ, line, start, i, line_num));
            continue;
        }
        if (line[i] == '%' && i + 1 < len && line[i + 1] == '=') {
            i += 2;
            tokens.push_back(make_token(TokenKind::PERCENT_EQ, line, start, i, line_num));
            continue;
        }

        // ---- ellipsis ----
        if (line[i] == '.' && i + 2 < len && line[i + 1] == '.' && line[i + 2] == '.') {
            i += 3;
            LOGX_DEBUG << "  [line " << line_num << "] ELLIPSIS";
            tokens.push_back(make_token(TokenKind::ELLIPSIS, line, start, i, line_num));
            continue;
        }

        // ---- single character tokens ----
        TokenKind single = TokenKind::UNKNOWN;
        switch (line[i]) {
            case '(': single = TokenKind::LPAREN;    break;
            case ')': single = TokenKind::RPAREN;    break;
            case '[': single = TokenKind::LSQUARE;   break;
            case ']': single = TokenKind::RSQUARE;   break;
            case '{': single = TokenKind::LCURLY;    break;
            case '}': single = TokenKind::RCURLY;    break;
            case ';': single = TokenKind::SEMICOLON; break;
            case ':': single = TokenKind::COLON;     break;
            case ',': single = TokenKind::COMMA;     break;
            case '.': single = TokenKind::DOT;       break;
            case '~': single = TokenKind::TILDE;     break;
            case '?': single = TokenKind::QUESTION;  break;
            case '+': single = TokenKind::PLUS;      break;
            case '-': single = TokenKind::MINUS;     break;
            case '*': single = TokenKind::STAR;      break;
            case '/': single = TokenKind::SLASH;     break;
            case '%': single = TokenKind::PERCENT;   break;
            case '&': single = TokenKind::AMPERSAND; break;
            case '|': single = TokenKind::PIPE;      break;
            case '^': single = TokenKind::CARET;     break;
            case '!': single = TokenKind::BANG;      break;
            case '=': single = TokenKind::EQUALS;    break;
            case '<': single = TokenKind::LT;        break;
            case '>': single = TokenKind::GT;        break;
            default:
                LOGX_WARN << "  [line " << line_num << "] UNKNOWN char: '" << line[i] << "'";
                i++;
                tokens.push_back(make_token(TokenKind::UNKNOWN, line, start, i, line_num));
                continue;
        }
        i++;
        LOGX_DEBUG << "  [line " << line_num << "] SYMBOL: '" << line[start] << "'";
        tokens.push_back(make_token(single, line, start, i, line_num));
    }

    LOGX_DEBUG << "line " << line_num << " produced " << tokens.size() << " tokens";
    return tokens;
}

}// namespace Lexer
