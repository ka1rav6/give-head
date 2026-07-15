#include "lexer.h"
#include <iostream>
#include <fstream>

namespace Lexer{
std::vector<std::string> read_file(std::string fileName) {
    std::vector<std::string> lines;
    std::ifstream file(fileName);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file: " << fileName << std::endl;
        return lines;
    }
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return lines;
}

static Declaration handle_preprocessor_directives(
        std::string line,
        size_t line_num ,
        size_t* i
    ){

    std::string directive = "";
    while (*i < line.size() && line[*i] != ' ')
        directive += line[*i++];
    if (directive == "include"){
        while (line[*i] == ' ') (*i)++;
        bool system = false;
        if (line[*i] == '<') 
            system = true;
        (*i)++;
        std::string path = "";
        if (system){
            while (*i < line.size() && line[*i] != '>')
                path += line[(*i)++];

            if (*i == line.size())
                std::cerr << "[ERROR] include doesnt end. line : " << line << "\nline number: " << line_num << std::endl;
            return IncludeStatement(true, path);
        }
        if (line[*i] == '\"'){
            while (*i < line.size() && line[*i] != '\"')
                path += line[(*i)++];
            if (*i == line.size())
                std::cerr << "[ERROR] include doesn't end. line : " << line << "\nline number: " << line_num << std::endl;
            return IncludeStatement(false, path);
        }
        std::cerr << "[ERROR] Illegal include statement. Line : " << line << "\nline number : " << line_num << std::endl;
        return IncludeStatement(false, "");
    }
    if (directive == "define"){
        std::string name = "";
        while (line[*i] == ' ') ++(*i);
        while (line[*i] != ' ')
            name += line[(*i)++];
        while (line[*i] == ' ') ++(*i);
        std::string params = "";
        std::string body = "";
        if (line[*i] == '('){ // macro has params
            while (*i < line.size() && line[*i] != ')')
                params += line[(*i)++];
            if (*i == line.size())
                std::cerr << "Macro parameters missing a paranthesis. Line : " << line << "\nLine number : " << line_num << std::endl;
            ++(*i);
        }
        while (*i != line.size()) 
            body += line[(*i)++];
        if (params == "") return Macro(name, body);
        return Macro(name, body, params);
    }
    std::cerr << "Preprocessor directive not defined!\nLine : " << line << "\nLine number : " << line_num << std::endl;
    return VariableDefinition(Type{"unknown", 0, 0}, ""); // temp
}



Declaration tokenise_line(std::string line, size_t line_num) {
    if (line.size() != 0)
        char ch = line.at(0);
    else 
        return VariableDefinition(Type{"unknown", 0, 0}, ""); // temp
    bool preprocessor_dir = false;
    if (line.size() != 0 && line.at(0) == '#')
        preprocessor_dir = true;
        
    for (size_t i = 0; i < line.size(); ++i){
        if (preprocessor_dir){
            i++;
            return handle_preprocessor_directives(line, line_num, &i);
        }
        
    }
    return VariableDefinition(Type{"unknown", 0, 0}, ""); // temp
}
} // Namespace lexer
