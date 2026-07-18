#include "generator.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace Generator {

static std::string make_include_guard(const std::string& filename) {
    std::string guard;
    for (char c : filename) {
        if (std::isalnum(static_cast<unsigned char>(c)))
            guard += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        else
            guard += '_';
    }
    return guard + "_H";
}

static void write_type(std::ostream& out, const Lexer::Type& t) {
    if (t.flags & Lexer::Flags::isStatic)   out << "static ";
    if (t.flags & Lexer::Flags::isExtern)   out << "extern ";
    if (t.flags & Lexer::Flags::isConst)    out << "const ";
    if (t.flags & Lexer::Flags::isVolitile) out << "volatile ";
    if (t.flags & Lexer::Flags::isSigned)   out << "signed ";
    out << t.baseType;
    for (uint32_t i = 0; i < t.pointerLevel; i++)
        out << "*";
}

static void write_params(std::ostream& out, const std::vector<Lexer::Parameter>& params) {
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) out << ", ";
        write_type(out, params[i].dataType);
        if (params[i].param_name.has_value())
            out << " " << *params[i].param_name;
    }
}

static void write_declaration(
    std::ostream& out,
    const Lexer::Declaration& decl,
    const std::string& indent)
{
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, Lexer::IncludeStatement>) {
            out << "#include " << (arg.system ? "<" : "\"")
                << arg.path << (arg.system ? ">" : "\"") << "\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::Macro>) {
            out << "#define " << arg.name;
            if (arg.parameters.has_value())
                out << *arg.parameters;
            out << " " << arg.body << "\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::ForwardDeclaration>) {
            const char* kw = "struct";
            if (arg.kind == Lexer::ForwardKind::Union) kw = "union";
            else if (arg.kind == Lexer::ForwardKind::Enum) kw = "enum";
            out << indent << kw << " " << arg.name << ";\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::Struct>) {
            out << indent << "struct " << arg.name << " {\n";
            for (const auto& f : arg.fields) {
                out << indent << "    ";
                write_type(out, f.type);
                out << " " << f.name << ";\n";
            }
            out << indent << "};\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::Union>) {
            out << indent << "union " << arg.name << " {\n";
            for (const auto& f : arg.fields) {
                out << indent << "    ";
                write_type(out, f.type);
                out << " " << f.name << ";\n";
            }
            out << indent << "};\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::Enum>) {
            out << indent << "enum " << arg.name << " {\n";
            for (size_t i = 0; i < arg.fields.size(); i++) {
                out << indent << "    " << arg.fields[i].name;
                if (arg.fields[i].value.has_value())
                    out << " = " << *arg.fields[i].value;
                if (i + 1 < arg.fields.size())
                    out << ",";
                out << "\n";
            }
            out << indent << "};\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::Typedef>) {
            out << indent << "typedef ";
            write_type(out, arg.underlyingType);
            out << " " << arg.alias << ";\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::ExternVariable>) {
            out << indent << "extern ";
            write_type(out, arg.type);
            out << " " << arg.name << ";\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::VariableDefinition>) {
            out << indent;
            write_type(out, arg.dataType);
            out << " " << arg.name;
            if (arg.value.has_value())
                out << " = " << *arg.value;
            out << ";\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::Function>) {
            out << indent;
            write_type(out, arg.returnType);
            out << " " << arg.name << "(";
            write_params(out, arg.params);
            out << ");\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::FunctionPointer>) {
            out << indent << "typedef ";
            write_type(out, arg.returnType);
            out << " (*" << arg.name << ")(";
            write_params(out, arg.params);
            out << ");\n";
        }
    }, decl);
}

void generate_header(
    const std::string& outputPath,
    const std::vector<Lexer::Declaration>& decls)
{
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        std::cerr << "Error: cannot open output file " << outputPath << std::endl;
        return;
    }

    // Strip path and extension to get base name for guard
    std::string baseName = outputPath;
    auto slashPos = baseName.find_last_of('/');
    if (slashPos != std::string::npos)
        baseName = baseName.substr(slashPos + 1);
    auto dotPos = baseName.find_last_of('.');
    if (dotPos != std::string::npos)
        baseName = baseName.substr(0, dotPos);

    std::string guard = make_include_guard(baseName);

    out << "/* This header is generated using copa \n"
        << "*  (https://github.com/ka1rav6/copa)\n"
        << "*/\n\n";
    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";

    for (const auto& decl : decls) {
        write_declaration(out, decl, "");
        out << "\n";
    }

    out << "#endif /* " << guard << " */\n";

    out.close();
    std::cout << "Generated header: " << outputPath << std::endl;
}

} // namespace Generator
