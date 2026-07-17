#include "lexer.h"
#include "parser.h"
#include <iostream>

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

    // Tokenize all lines
    std::vector<std::vector<Lexer::Token>> line_tokens;
    for (size_t i = 0; i < lines.size(); i++) {
        line_tokens.push_back(Lexer::tokenise_line(lines[i], i + 1));
    }

    // Flatten into a single token stream
    std::vector<Lexer::Token> tokens = Parser::flatten_tokens(line_tokens);

    // Parse declarations
    std::vector<Lexer::Declaration> decls = Parser::parse(tokens);

    // Print results
    std::cout << "=== Parsed " << decls.size() << " declarations ===" << std::endl;
    for (const auto& decl : decls) {
        std::cout << "[" << Parser::declaration_kind_name(decl) << "] ";
        Parser::print_declaration(decl);
    }

    return 0;
}
