#include "generator.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <set>

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

static void write_struct_body(std::ostream& out, const std::string& name,
                               const std::vector<Lexer::StructField>& fields,
                               const std::string& indent) {
    out << indent << "struct " << name << " {\n";
    for (const auto& f : fields) {
        out << indent << "    ";
        write_type(out, f.type);
        out << " " << f.name << ";\n";
    }
    out << indent << "};\n";
}

static void write_union_body(std::ostream& out, const std::string& name,
                              const std::vector<Lexer::UnionField>& fields,
                              const std::string& indent) {
    out << indent << "union " << name << " {\n";
    for (const auto& f : fields) {
        out << indent << "    ";
        write_type(out, f.type);
        out << " " << f.name << ";\n";
    }
    out << indent << "};\n";
}

static void write_enum_body(std::ostream& out, const std::string& name,
                             const std::vector<Lexer::EnumField>& fields,
                             const std::string& indent) {
    out << indent << "enum " << name << " {\n";
    for (size_t i = 0; i < fields.size(); i++) {
        out << indent << "    " << fields[i].name;
        if (fields[i].value.has_value())
            out << " = " << *fields[i].value;
        if (i + 1 < fields.size())
            out << ",";
        out << "\n";
    }
    out << indent << "};\n";
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
            write_struct_body(out, arg.name, arg.fields, indent);
        }
        else if constexpr (std::is_same_v<T, Lexer::Union>) {
            write_union_body(out, arg.name, arg.fields, indent);
        }
        else if constexpr (std::is_same_v<T, Lexer::Enum>) {
            write_enum_body(out, arg.name, arg.fields, indent);
        }
        else if constexpr (std::is_same_v<T, Lexer::Typedef>) {
            out << indent << "typedef ";
            write_type(out, arg.underlyingType);
            out << " " << arg.alias << ";\n";
        }
        else if constexpr (std::is_same_v<T, Lexer::ExternVariable>) {
            out << indent;
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

    std::set<std::string> emitted_functions;
    std::set<size_t> skip;

    // Mark anonymous struct/union/enum + typedef pairs for combined output
    // Pattern: Struct{name=""} followed by Typedef{baseType="struct", alias=Name}
    for (size_t i = 0; i + 1 < decls.size(); i++) {
        if (std::holds_alternative<Lexer::Struct>(decls[i]) &&
            std::holds_alternative<Lexer::Typedef>(decls[i + 1]))
        {
            const auto& s = std::get<Lexer::Struct>(decls[i]);
            const auto& t = std::get<Lexer::Typedef>(decls[i + 1]);
            if (s.name.empty() && t.underlyingType.baseType == "struct") {
                skip.insert(i);
            }
        }
        else if (std::holds_alternative<Lexer::Union>(decls[i]) &&
                 std::holds_alternative<Lexer::Typedef>(decls[i + 1]))
        {
            const auto& u = std::get<Lexer::Union>(decls[i]);
            const auto& t = std::get<Lexer::Typedef>(decls[i + 1]);
            if (u.name.empty() && t.underlyingType.baseType == "union") {
                skip.insert(i);
            }
        }
        else if (std::holds_alternative<Lexer::Enum>(decls[i]) &&
                 std::holds_alternative<Lexer::Typedef>(decls[i + 1]))
        {
            const auto& e = std::get<Lexer::Enum>(decls[i]);
            const auto& t = std::get<Lexer::Typedef>(decls[i + 1]);
            if (e.name.empty() && t.underlyingType.baseType == "enum") {
                skip.insert(i);
            }
        }
    }

    for (size_t i = 0; i < decls.size(); i++) {
        if (skip.count(i))
            continue;

        const auto& decl = decls[i];

        // Combined anonymous typedef struct/union/enum
        if (std::holds_alternative<Lexer::Typedef>(decl) &&
            (std::get<Lexer::Typedef>(decl).underlyingType.baseType == "struct" ||
             std::get<Lexer::Typedef>(decl).underlyingType.baseType == "union" ||
             std::get<Lexer::Typedef>(decl).underlyingType.baseType == "enum"))
        {
            // Check if previous decl is the matching anonymous body
            if (i > 0 && skip.count(i - 1)) {
                const auto& prev = decls[i - 1];
                const auto& td = std::get<Lexer::Typedef>(decl);

                if (td.underlyingType.baseType == "struct" &&
                    std::holds_alternative<Lexer::Struct>(prev))
                {
                    const auto& s = std::get<Lexer::Struct>(prev);
                    out << "typedef struct {\n";
                    for (const auto& f : s.fields) {
                        out << "    ";
                        write_type(out, f.type);
                        out << " " << f.name << ";\n";
                    }
                    out << "} " << td.alias << ";\n";
                    continue;
                }
                else if (td.underlyingType.baseType == "union" &&
                         std::holds_alternative<Lexer::Union>(prev))
                {
                    const auto& u = std::get<Lexer::Union>(prev);
                    out << "typedef union {\n";
                    for (const auto& f : u.fields) {
                        out << "    ";
                        write_type(out, f.type);
                        out << " " << f.name << ";\n";
                    }
                    out << "} " << td.alias << ";\n";
                    continue;
                }
                else if (td.underlyingType.baseType == "enum" &&
                         std::holds_alternative<Lexer::Enum>(prev))
                {
                    const auto& e = std::get<Lexer::Enum>(prev);
                    out << "typedef enum {\n";
                    for (size_t j = 0; j < e.fields.size(); j++) {
                        out << "    " << e.fields[j].name;
                        if (e.fields[j].value.has_value())
                            out << " = " << *e.fields[j].value;
                        if (j + 1 < e.fields.size())
                            out << ",";
                        out << "\n";
                    }
                    out << "} " << td.alias << ";\n";
                    continue;
                }
            }
            // Fallback: emit as regular typedef
            write_declaration(out, decl, "");
            out << "\n";
            continue;
        }

        // Deduplicate function declarations (prototype + definition)
        if (std::holds_alternative<Lexer::Function>(decl)) {
            const auto& fn = std::get<Lexer::Function>(decl);
            if (emitted_functions.count(fn.name))
                continue;
            emitted_functions.insert(fn.name);
        }

        write_declaration(out, decl, "");
        out << "\n";
    }

    out << "#endif /* " << guard << " */\n";

    out.close();
    std::cout << "Generated header: " << outputPath << std::endl;
}

} // namespace Generator
