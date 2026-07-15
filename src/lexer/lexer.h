#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include "token_definitions.h"

namespace Lexer{
std::vector<std::string> read_file(std::string fileName);
Declaration tokenise_line(std::string line, size_t line_num);


}


#endif // LEXER_H
