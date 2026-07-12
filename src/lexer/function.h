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
};

enum Flags{
    isStatic = 1 << 0,
    isExtern = 1 << 1,
};

struct Function{
    std::string returnType;
    std::string name;
    std::vector<Parameter> params;
    uint8_t flags;
};



#endif // FUNCTION_H
