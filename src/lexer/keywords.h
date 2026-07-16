#ifndef KEYWORDS_H
#define KEYWORDS_H

#include <string_view>
#include <algorithm>
#include <vector>

namespace Lexer{

static const std::vector<std::string> c_keywords = {
    // C89/C90
    "auto", "break", "case", "char", "const", "continue",
    "default", "do", "double", "else", "enum", "extern",
    "float", "for", "goto", "if", "int", "long",
    "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void",
    "volatile", "while",

    // C99
    "bool", "complex", "imaginary",
    "inline", "restrict",

    // C11
    "_Alignas", "_Alignof", "_Atomic",
    "_Bool", "_Complex", "_Generic",
    "_Imaginary", "_Noreturn", "_Static_assert",
    "_Thread_local",

    // common extensions / typedefs used as keywords
    "size_t", "ssize_t", "ptrdiff_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "intptr_t", "uintptr_t",
    "FILE", "NULL", "EOF",
};

static bool is_keyword(std::string_view word) {
    for (const auto& kw : c_keywords) {
        if (kw == word)
            return true;
    }
    return false;
}

} // namespace Lexer

#endif // KEYWORDS_H
