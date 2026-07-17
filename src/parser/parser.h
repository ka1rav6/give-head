#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <string>
#include "token_definitions.h"

namespace Parser {

// Flattens per-line token vectors into a single token stream.
// Strips LINE_COMMENT, BLOCK_COMMENT, and NEWLINE tokens.
std::vector<Lexer::Token> flatten_tokens(
    const std::vector<std::vector<Lexer::Token>>& line_tokens);

// Parses a flat token stream into a list of top-level declarations.
std::vector<Lexer::Declaration> parse(
    const std::vector<Lexer::Token>& tokens);

// Returns a human-readable name for a Declaration variant.
std::string declaration_kind_name(const Lexer::Declaration& decl);

// Pretty-prints a declaration to stdout.
void print_declaration(const Lexer::Declaration& decl);

} // namespace Parser

#endif // PARSER_H
