#ifndef FUNCTION_H
#define FUNCTION_H

#include <string>
#include <vector>
#include <cstdint>
#include <iostream>
#include <optional>

struct Parameter{
    std::string dataType;
    std::optional<std::string> param_name;
    Parameter(std::string type, std::string name){
        this->dataType = type;
        this->param_name = name;
    }
};

enum Flags{
    isStatic  = 1 << 0, // for functions
    isExtern  = 1 << 1, // for functions
    isTypedef = 1 << 2, // for enums/structs etc
};

struct Function{
    std::string name;
    std::vector<Parameter> params;
    std::string returnType;
    uint8_t flags;
};

struct EnumField{
    std::string name;
    std::optional<std::string> value;
};

struct Enum{
    std::string name;
    std::vector<EnumField> fields;
};

struct ExternVariable{
    std::string type;
    std::string name;
};

struct Struct{
    std::string name;
    std::string fields; // as C structs have no initalization
};




#endif // FUNCTION_H
