#include "parser.h"
#include "logx_linux.h"
#include <iostream>
#include <stdexcept>
#include <sstream>

namespace Parser {

// ---------------------------------------------------------------------------
// Token stream helpers
// ---------------------------------------------------------------------------

std::vector<Lexer::Token> flatten_tokens(
    const std::vector<std::vector<Lexer::Token>>& line_tokens)
{
    size_t total = 0;
    for (const auto& line : line_tokens) total += line.size();
    LOGX_DEBUG << "flattening " << line_tokens.size() << " lines, "
               << total << " raw tokens";

    std::vector<Lexer::Token> out;
    for (const auto& line : line_tokens) {
        for (const auto& tok : line) {
            if (tok.kind == Lexer::TokenKind::LINE_COMMENT ||
                tok.kind == Lexer::TokenKind::BLOCK_COMMENT ||
                tok.kind == Lexer::TokenKind::NEWLINE)
                continue;
            out.push_back(tok);
        }
    }
    LOGX_DEBUG << "flattened to " << out.size() << " tokens (stripped "
               << (total - out.size()) << " comments/newlines)";
    return out;
}

// ---------------------------------------------------------------------------
// Parser state
// ---------------------------------------------------------------------------

struct Parser {
    const std::vector<Lexer::Token>& tokens;
    size_t pos = 0;

    explicit Parser(const std::vector<Lexer::Token>& t) : tokens(t) {}

    bool at_end() const { return pos >= tokens.size(); }

    const Lexer::Token& peek() const {
        if (at_end())
            throw std::runtime_error("unexpected end of input");
        return tokens[pos];
    }

    const Lexer::Token& advance() {
        if (at_end())
            throw std::runtime_error("unexpected end of input");
        return tokens[pos++];
    }

    bool check(Lexer::TokenKind kind) const {
        return !at_end() && peek().kind == kind;
    }

    bool match(Lexer::TokenKind kind) {
        if (check(kind)) {
            advance();
            return true;
        }
        return false;
    }

    const Lexer::Token& expect(Lexer::TokenKind kind, const char* msg) {
        if (!check(kind)) {
            std::ostringstream oss;
            oss << msg << " at line " << peek().line_num
                << " col " << peek().col_num
                << ": got '" << peek().raw << "'";
            throw std::runtime_error(oss.str());
        }
        return advance();
    }

    bool check_kw(const std::string& text) const {
        return !at_end() && peek().kind == Lexer::TokenKind::KEYWORD
            && peek().raw == text;
    }

    void skip_to_semicolon() {
        while (!at_end() && !check(Lexer::TokenKind::SEMICOLON))
            advance();
        match(Lexer::TokenKind::SEMICOLON);
    }
};

// ---------------------------------------------------------------------------
// Forward declarations of parse helpers
// ---------------------------------------------------------------------------

static Lexer::Type parse_type(Parser& p);
static std::vector<Lexer::Parameter> parse_param_list(Parser& p);
static std::string parse_expression(Parser& p);
static std::string parse_array_dims(Parser& p);
static void parse_typedef(Parser& p, std::vector<Lexer::Declaration>& decls);

// ---------------------------------------------------------------------------
// Type parsing
// ---------------------------------------------------------------------------

static bool is_type_keyword(const std::string& word) {
    static const char* type_words[] = {
        "void", "char", "short", "int", "long", "float", "double",
        "signed", "unsigned",
        "bool", "_Bool", "size_t", "ssize_t", "ptrdiff_t",
        "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        "intptr_t", "uintptr_t", "FILE",
    };
    for (const auto* w : type_words)
        if (word == w) return true;
    return false;
}

static Lexer::Type parse_type(Parser& p) {
    Lexer::Type type;
    type.pointerLevel = 0;
    type.flags = 0;

    // Consume qualifiers and storage class keywords
    while (!p.at_end() && p.peek().kind == Lexer::TokenKind::KEYWORD) {
        const std::string& w = p.peek().raw;

        // Qualifiers / storage classes that set flags
        if      (w == "const")    {
            type.flags |= Lexer::Flags::isConst;
            p.advance();
        }
        else if (w == "volatile") {
            type.flags |= Lexer::Flags::isVolitile;
            p.advance();
        }
        else if (w == "static")   {
            type.flags |= Lexer::Flags::isStatic;
            p.advance();
        }
        else if (w == "extern")   {
            type.flags |= Lexer::Flags::isExtern;
            p.advance();
        }
        else if (w == "signed")   {
            type.flags |= Lexer::Flags::isSigned;
            p.advance();
        }
        else if (w == "unsigned") {
            type.baseType += "unsigned ";
            p.advance();
        }
        // Type-name keywords: int, char, void, double, float, long, short, etc.
        else if (is_type_keyword(w)) {
            if (!type.baseType.empty() && type.baseType.back() != ' ')
                type.baseType += ' ';
            type.baseType += w;
            p.advance();
        }
        // struct / union / enum tags
        else if (w == "struct" || w == "union" || w == "enum") {
            type.baseType = w;
            p.advance();
            if (p.check(Lexer::TokenKind::IDENTIFIER))
                type.baseType += " " + p.advance().raw;
        }
        else break;
    }

    // After struct/union/enum keyword, consume optional tag name
    if ((type.baseType == "struct" || type.baseType == "union" ||
         type.baseType == "enum") &&
        !p.at_end() && p.peek().kind == Lexer::TokenKind::IDENTIFIER)
    {
        type.baseType += " " + p.advance().raw;
    }

    // Plain identifier type (typedef name, user type, etc.)
    if (type.baseType.empty() && !p.at_end()
        && p.peek().kind == Lexer::TokenKind::IDENTIFIER)
    {
        type.baseType = p.advance().raw;
    }

    // Trim trailing space
    while (!type.baseType.empty() && type.baseType.back() == ' ')
        type.baseType.pop_back();

    // Count pointer stars, also consuming any qualifiers that follow a
    // star (e.g. "char* const", "int* const*"). Type only tracks a single
    // flag set rather than per-pointer-level qualifiers, so these are
    // folded into the same flags -- but consuming them here is essential:
    // previously an unconsumed trailing "const" caused the caller's
    // IDENTIFIER check to fail, which silently discarded the entire
    // declaration (e.g. "extern const char* const program_name;").
    while (true) {
        if (p.match(Lexer::TokenKind::STAR)) {
            type.pointerLevel++;
            continue;
        }
        if (p.check_kw("const")) {
            type.flags |= Lexer::Flags::isConst;
            p.advance();
            continue;
        }
        if (p.check_kw("volatile")) {
            type.flags |= Lexer::Flags::isVolitile;
            p.advance();
            continue;
        }
        break;
    }

    return type;
}

// ---------------------------------------------------------------------------
// Parameter list parsing
// ---------------------------------------------------------------------------
static std::vector<Lexer::Parameter> parse_param_list(Parser& p) {
    std::vector<Lexer::Parameter> params;
    p.expect(Lexer::TokenKind::LPAREN, "expected '('");
    LOGX_DEBUG << "        parse_param_list: entered";

    // (void) means an explicit empty parameter list. This is kept distinct
    // from a bare "()" (which in a C declaration means "unspecified
    // parameters", not "zero parameters") by recording a single "void"
    // parameter, which write_params() renders back out as literal "void".
    if (p.check_kw("void")) {
        size_t saved = p.pos;
        p.advance();
        if (p.check(Lexer::TokenKind::RPAREN)) {
            LOGX_DEBUG << "        parse_param_list: (void) -> explicit void";
            p.advance();
            params.emplace_back(Lexer::Type{"void", 0, 0}, std::nullopt);
            return params;
        }
        p.pos = saved;
    }

    if (p.check(Lexer::TokenKind::RPAREN)) {
        LOGX_DEBUG << "        parse_param_list: empty () -> 0 params";
        p.advance();
        return params;
    }

    while (!p.at_end() && !p.check(Lexer::TokenKind::RPAREN)) {
        // Handle variadic: ...
        if (p.check(Lexer::TokenKind::ELLIPSIS)) {
            LOGX_DEBUG << "        parse_param_list: found variadic ...";
            p.advance();
            break;
        }

        Lexer::Type param_type = parse_type(p);
        std::optional<std::string> param_name;
        bool handled = false;

        // Function pointer parameter: RetType (*name)(params)
        // e.g. void register_callback(int id, void (*cb)(void));
        if (p.check(Lexer::TokenKind::LPAREN)) {
            size_t afterParen = p.pos + 1;
            if (afterParen < p.tokens.size() &&
                p.tokens[afterParen].kind == Lexer::TokenKind::STAR)
            {
                size_t afterStar = afterParen + 1;
                size_t afterName = afterStar;
                if (afterName < p.tokens.size() &&
                    p.tokens[afterName].kind == Lexer::TokenKind::IDENTIFIER)
                    afterName++;
                if (afterName < p.tokens.size() &&
                    p.tokens[afterName].kind == Lexer::TokenKind::RPAREN)
                {
                    p.advance(); // (
                    p.advance(); // *
                    std::string fpName;
                    if (p.check(Lexer::TokenKind::IDENTIFIER))
                        fpName = p.advance().raw;
                    p.expect(Lexer::TokenKind::RPAREN, "expected ')'");
                    auto fparams = parse_param_list(p);

                    std::ostringstream oss;
                    oss << param_type.baseType;
                    for (uint32_t s = 0; s < param_type.pointerLevel; s++) oss << "*";
                    oss << " (*" << fpName << ")(";
                    for (size_t i = 0; i < fparams.size(); i++) {
                        if (i) oss << ", ";
                        oss << fparams[i].dataType.baseType;
                        for (uint32_t s = 0; s < fparams[i].dataType.pointerLevel; s++) oss << "*";
                        if (fparams[i].param_name) oss << " " << *fparams[i].param_name;
                    }
                    oss << ")";

                    LOGX_DEBUG << "        parse_param_list: function-pointer param -> " << oss.str();
                    params.emplace_back(Lexer::Type{oss.str(), 0, 0}, std::nullopt);
                    handled = true;
                }
            }
        }

        if (!handled) {
            if (p.check(Lexer::TokenKind::IDENTIFIER))
                param_name = p.advance().raw;

            // Array parameter: name[] or name[][3] etc. Arrays decay to
            // pointers as function parameters, but reproducing the original
            // bracket syntax (rather than collapsing to a bare pointer) keeps
            // multi-dimensional array parameters like "int rows[][3]" valid
            // -- a single bracket-skip previously left the second "[3]"
            // unconsumed, which broke parsing of the rest of the signature.
            std::string dims = parse_array_dims(p);
            if (!dims.empty())
                param_name = param_name.value_or("") + dims;

            LOGX_DEBUG << "        parse_param_list: param " << params.size() << " = "
                       << param_type.baseType << (param_name ? " " + *param_name : " (unnamed)");
            params.emplace_back(std::move(param_type), std::move(param_name));
        }

        if (!p.match(Lexer::TokenKind::COMMA))
            break;
    }

    p.expect(Lexer::TokenKind::RPAREN, "expected ')'");
    return params;
}

// ---------------------------------------------------------------------------
// Expression parsing (minimal — collects tokens until a delimiter)
// ---------------------------------------------------------------------------

static std::string parse_expression(Parser& p) {
    std::string result;
    int brace_depth = 0;
    int paren_depth = 0;
    int square_depth = 0;

    while (!p.at_end()) {
        const auto& tok = p.peek();
        if (tok.kind == Lexer::TokenKind::LCURLY) brace_depth++;
        if (tok.kind == Lexer::TokenKind::RCURLY) {
            if (brace_depth == 0) break;
            brace_depth--;
        }
        if (tok.kind == Lexer::TokenKind::LPAREN) paren_depth++;
        if (tok.kind == Lexer::TokenKind::RPAREN) {
            if (paren_depth == 0) break;
            paren_depth--;
        }
        if (tok.kind == Lexer::TokenKind::LSQUARE) square_depth++;
        if (tok.kind == Lexer::TokenKind::RSQUARE) {
            if (square_depth == 0) break;
            square_depth--;
        }
        if (brace_depth == 0 && paren_depth == 0 && square_depth == 0) {
            if (tok.kind == Lexer::TokenKind::COMMA ||
                tok.kind == Lexer::TokenKind::SEMICOLON)
                break;
        }
        if (!result.empty()) result += ' ';
        result += tok.raw;
        p.advance();
    }
    return result;
}

// ---------------------------------------------------------------------------
// Array-dimension suffix parsing: consumes zero or more [expr] groups and
// returns them concatenated as raw text (e.g. "[4][4]"), so callers can
// preserve the original array declarator instead of collapsing it into a
// pointer or silently dropping it.
// ---------------------------------------------------------------------------
static std::string parse_array_dims(Parser& p) {
    std::string dims;
    while (p.check(Lexer::TokenKind::LSQUARE)) {
        p.advance();
        dims += "[";
        while (!p.check(Lexer::TokenKind::RSQUARE) && !p.at_end())
            dims += p.advance().raw;
        p.expect(Lexer::TokenKind::RSQUARE, "expected ']'");
        dims += "]";
    }
    return dims;
}

// ---------------------------------------------------------------------------
// Macro / include parsing (re-parse the PREPROCESSOR token's raw text)
// ---------------------------------------------------------------------------

static Lexer::Declaration parse_preprocessor(Parser& p) {
    const auto& tok = p.advance();
    const std::string& raw = tok.raw;

    size_t start = 1;
    while (start < raw.size() && (raw[start] == ' ' || raw[start] == '\t'))
        start++;

    size_t end = start;
    while (end < raw.size() && raw[end] != ' ' && raw[end] != '\t' &&
           raw[end] != '(' && raw[end] != '\n')
        end++;

    std::string directive = raw.substr(start, end - start);
    LOGX_DEBUG << "    preprocessor directive: " << directive;

    if (directive == "include") {
        size_t rest = end;
        while (rest < raw.size() && (raw[rest] == ' ' || raw[rest] == '\t'))
            rest++;
        if (rest < raw.size()) {
            bool system = raw[rest] == '<';
            char delim = system ? '>' : '"';
            size_t path_start = rest + 1;
            size_t path_end = raw.find(delim, path_start);
            if (path_end == std::string::npos) path_end = raw.size();
            std::string path = raw.substr(path_start, path_end - path_start);
            LOGX_DEBUG << "    include path: " << path << (system ? " (system)" : " (local)");
            return Lexer::IncludeStatement(system, std::move(path));
        }
    }

    if (directive == "define") {
        size_t rest = end;
        while (rest < raw.size() && (raw[rest] == ' ' || raw[rest] == '\t'))
            rest++;

        size_t name_start = rest;
        while (rest < raw.size() && raw[rest] != '(' && raw[rest] != ' ' &&
               raw[rest] != '\t' && raw[rest] != '\n')
            rest++;

        std::string name = raw.substr(name_start, rest - name_start);
        LOGX_DEBUG << "    macro name: " << name;

        std::optional<std::string> params;
        while (rest < raw.size() && (raw[rest] == ' ' || raw[rest] == '\t'))
            rest++;

        if (rest < raw.size() && raw[rest] == '(') {
            size_t paren_start = rest;
            int depth = 1;
            rest++;
            while (rest < raw.size() && depth > 0) {
                if (raw[rest] == '(') depth++;
                if (raw[rest] == ')') depth--;
                rest++;
            }
            params = raw.substr(paren_start, rest - paren_start);
        }

        while (rest < raw.size() && (raw[rest] == ' ' || raw[rest] == '\t'))
            rest++;

        std::string body;
        if (rest < raw.size()) {
            body = raw.substr(rest);
            while (!body.empty() && (body.back() == ' ' || body.back() == '\t'))
                body.pop_back();
        }

        return Lexer::Macro(std::move(name), std::move(body), std::move(params));
    }

    return Lexer::Macro(directive, raw.substr(start));
}

// ---------------------------------------------------------------------------
// Forward declaration detection
// ---------------------------------------------------------------------------

static bool looks_like_forward_decl(Parser& p) {
    if (!p.check_kw("struct") && !p.check_kw("union") && !p.check_kw("enum"))
        return false;

    size_t scan = p.pos + 1;
    if (scan >= p.tokens.size() ||
        p.tokens[scan].kind != Lexer::TokenKind::IDENTIFIER)
        return false;

    scan++;
    while (scan < p.tokens.size() &&
           p.tokens[scan].kind == Lexer::TokenKind::IDENTIFIER)
        scan++;

    return scan < p.tokens.size() &&
           p.tokens[scan].kind == Lexer::TokenKind::SEMICOLON;
}

// ---------------------------------------------------------------------------
// Struct / Union parsing
// ---------------------------------------------------------------------------

// Skip __attribute__((...)) sequences (GCC/Clang extension).
// Expects p to be pointing AT `__attribute__`.
// Returns the raw "__attribute__((...))" text if present (and consumes
// it), or an empty string otherwise. Previously this text was discarded
// entirely, which for something like __attribute__((packed)) silently
// changes the struct's layout/ABI between the .c source and the
// generated header.
static std::string capture_attribute(Parser& p) {
    if (p.at_end() || p.peek().raw != "__attribute__")
        return "";
    std::string text = p.advance().raw; // __attribute__
    if (p.match(Lexer::TokenKind::LPAREN)) {
        text += "(";
        int depth = 1;
        while (!p.at_end() && depth > 0) {
            if (p.check(Lexer::TokenKind::LPAREN)) depth++;
            if (p.check(Lexer::TokenKind::RPAREN)) depth--;
            text += p.peek().raw;
            p.advance();
        }
    }
    return text;
}

static void skip_attribute(Parser& p) {
    capture_attribute(p);
}

// Captures the text of a brace-delimited block (e.g. a nested anonymous
// struct/union body) instead of discarding it, so a field like
// "struct { size_t a; size_t b; } inner;" keeps its member list rather
// than collapsing to "struct inner;" with the body silently dropped.
// Expects p to be pointing AT the opening LCURLY.
static std::string capture_brace_block_text(Parser& p) {
    std::string out = "{\n";
    if (!p.check(Lexer::TokenKind::LCURLY))
        return out + "}";
    p.advance(); // consume opening {
    int depth = 1;
    std::string line;
    auto flush_line = [&]() {
        if (!line.empty()) {
            out += "        " + line + "\n";
            line.clear();
        }
    };
    while (!p.at_end() && depth > 0) {
        const auto& tok = p.peek();
        if (tok.kind == Lexer::TokenKind::LCURLY) {
            depth++;
            line += line.empty() ? "{" : " {";
            p.advance();
            continue;
        }
        if (tok.kind == Lexer::TokenKind::RCURLY) {
            depth--;
            p.advance();
            if (depth == 0) break;
            line += " }";
            continue;
        }
        if (!line.empty() &&
            tok.kind != Lexer::TokenKind::SEMICOLON &&
            tok.kind != Lexer::TokenKind::COMMA)
            line += " ";
        line += tok.raw;
        if (tok.kind == Lexer::TokenKind::SEMICOLON)
            flush_line();
        p.advance();
    }
    flush_line();
    out += "    }";
    return out;
}

// Parses the declarator portion of a struct/union field -- the part
// after the base type has already been parsed into `ft`: nested
// anonymous struct/union bodies, function-pointer fields, plain names,
// array dimensions, and bitfield widths. Returns the field's name
// (which, for a function-pointer field, embeds the "(*name)(params)"
// declarator so the generator reproduces it verbatim instead of the
// function-pointer signature being discarded).
static std::string parse_aggregate_field_declarator(Parser& p, Lexer::Type& ft) {
    // Nested anonymous struct/union: struct/union { ... } [*]fieldname;
    // Previously the body was thrown away via skip_brace_block(), turning
    // e.g. "struct { size_t a; size_t b; } inner;" into "struct inner;"
    // with the member list silently dropped.
    if ((ft.baseType == "struct" || ft.baseType == "union") &&
        p.check(Lexer::TokenKind::LCURLY))
    {
        ft.baseType += " " + capture_brace_block_text(p);
        ft.pointerLevel = 0;
        while (p.match(Lexer::TokenKind::STAR))
            ft.pointerLevel++;
    }

    std::string fname;

    // Function pointer field: RetType (*name)(params)
    // Previously the parameter list was parsed and discarded, and the
    // field's name/signature was lost entirely (e.g.
    // "void (*on_event)(void* data);" became "void on_event;").
    if (p.check(Lexer::TokenKind::LPAREN)) {
        size_t peek1 = p.pos + 1;
        if (peek1 < p.tokens.size() &&
            p.tokens[peek1].kind == Lexer::TokenKind::STAR)
        {
            p.advance(); // (
            p.advance(); // *
            std::string fpName;
            if (p.check(Lexer::TokenKind::IDENTIFIER))
                fpName = p.advance().raw;
            p.expect(Lexer::TokenKind::RPAREN, "expected ')'");
            auto fparams = parse_param_list(p);

            std::ostringstream oss;
            oss << "(*" << fpName << ")(";
            for (size_t i = 0; i < fparams.size(); i++) {
                if (i) oss << ", ";
                oss << fparams[i].dataType.baseType;
                for (uint32_t s = 0; s < fparams[i].dataType.pointerLevel; s++) oss << "*";
                if (fparams[i].param_name) oss << " " << *fparams[i].param_name;
            }
            oss << ")";
            fname = oss.str();
        }
    }

    if (fname.empty() && p.check(Lexer::TokenKind::IDENTIFIER))
        fname = p.advance().raw;

    // Array dimensions: name[size][size]... (previously a single bracket
    // group was skipped without preserving the size at all).
    fname += parse_array_dims(p);

    // Skip bitfield width: field : 8;
    if (p.match(Lexer::TokenKind::COLON)) {
        while (!p.at_end() && !p.check(Lexer::TokenKind::SEMICOLON))
            p.advance();
    }

    return fname;
}

static Lexer::Declaration parse_struct_or_union(Parser& p) {
    bool is_struct = p.check_kw("struct");
    p.advance();

    std::string attr = capture_attribute(p);

    std::string name;
    if (p.check(Lexer::TokenKind::IDENTIFIER))
        name = p.advance().raw;

    std::string trailing_attr = capture_attribute(p);
    if (!trailing_attr.empty())
        attr = attr.empty() ? trailing_attr : attr + " " + trailing_attr;

    // Fold the attribute text into the declared name so it survives
    // through to the generator, which prints "struct " << name << " {".
    // This preserves e.g. "struct __attribute__((packed)) PackedHeader"
    // without needing to plumb a new field through Struct/Union/generator.
    if (!attr.empty())
        name = name.empty() ? attr : attr + " " + name;

    LOGX_DEBUG << "    parsing " << (is_struct ? "struct" : "union") << " " << name;
    p.expect(Lexer::TokenKind::LCURLY, "expected '{'");

    if (is_struct) {
        std::vector<Lexer::StructField> fields;
        while (!p.at_end() && !p.check(Lexer::TokenKind::RCURLY)) {

            size_t loop_start = p.pos;
            Lexer::Type ft = parse_type(p);
            std::string fname = parse_aggregate_field_declarator(p, ft);
            p.match(Lexer::TokenKind::SEMICOLON);
            fields.emplace_back(std::move(ft), std::move(fname));

            if (p.pos == loop_start) p.advance();
        }
        p.expect(Lexer::TokenKind::RCURLY, "expected '}'");
        p.match(Lexer::TokenKind::SEMICOLON);
        return Lexer::Struct(std::move(name), std::move(fields));
    } else {
        std::vector<Lexer::UnionField> fields;
        while (!p.at_end() && !p.check(Lexer::TokenKind::RCURLY)) {
            size_t loop_start = p.pos;
            Lexer::Type ft = parse_type(p);
            std::string fname = parse_aggregate_field_declarator(p, ft);
            p.match(Lexer::TokenKind::SEMICOLON);
            fields.emplace_back(std::move(fname), std::move(ft));

            if (p.pos == loop_start) p.advance();
        }
        p.expect(Lexer::TokenKind::RCURLY, "expected '}'");
        p.match(Lexer::TokenKind::SEMICOLON);
        return Lexer::Union(std::move(name), std::move(fields));
    }
}

// ---------------------------------------------------------------------------
// Enum parsing
// ---------------------------------------------------------------------------

static Lexer::Declaration parse_enum(Parser& p) {
    p.advance();
    std::string name;
    if (p.check(Lexer::TokenKind::IDENTIFIER))
        name = p.advance().raw;

    LOGX_DEBUG << "      parsing enum " << name;
    p.expect(Lexer::TokenKind::LCURLY, "expected '{'");

    std::vector<Lexer::EnumField> fields;
    while (!p.at_end() && !p.check(Lexer::TokenKind::RCURLY)) {
        std::string field_name;
        if (p.check(Lexer::TokenKind::IDENTIFIER))
            field_name = p.advance().raw;

        std::optional<std::string> value;
        if (p.match(Lexer::TokenKind::EQUALS))
            value = parse_expression(p);

        LOGX_DEBUG << "        enum field: " << field_name
                   << (value ? " = " + *value : "");
        fields.emplace_back(std::move(field_name), std::move(value));
        p.match(Lexer::TokenKind::COMMA);
    }

    LOGX_DEBUG << "      enum " << name << " has " << fields.size() << " fields";
    p.expect(Lexer::TokenKind::RCURLY, "expected '}'");
    p.match(Lexer::TokenKind::SEMICOLON);
    return Lexer::Enum(std::move(name), std::move(fields));
}

// ---------------------------------------------------------------------------
// Typedef parsing (includes function pointers and anonymous struct/enum)
// ---------------------------------------------------------------------------

static void parse_typedef(Parser& p, std::vector<Lexer::Declaration>& decls) {
    p.advance(); // typedef
    LOGX_DEBUG << "    parse_typedef: entered";

    size_t saved = p.pos;

    Lexer::Type underlying = parse_type(p);

    // Function pointer typedef: typedef RetType (*alias)(params);
    if (p.check(Lexer::TokenKind::LPAREN)) {
        size_t inner = p.pos + 1;
        if (inner < p.tokens.size() &&
            p.tokens[inner].kind == Lexer::TokenKind::STAR)
        {
            size_t inner2 = inner + 1;
            if (inner2 < p.tokens.size() &&
                p.tokens[inner2].kind == Lexer::TokenKind::IDENTIFIER)
            {
                p.expect(Lexer::TokenKind::LPAREN, "expected '('");
                p.expect(Lexer::TokenKind::STAR, "expected '*'");
                std::string alias = p.advance().raw;
                p.expect(Lexer::TokenKind::RPAREN, "expected ')'");

                // Pointer-to-array typedef: typedef RetType (*alias)[N];
                // (as opposed to a function pointer typedef, which is
                // followed by a parameter list in parentheses).
                if (p.check(Lexer::TokenKind::LSQUARE)) {
                    std::string arrayAlias = "(*" + alias + ")";
                    while (p.match(Lexer::TokenKind::LSQUARE)) {
                        arrayAlias += "[";
                        while (!p.check(Lexer::TokenKind::RSQUARE) && !p.at_end())
                            arrayAlias += p.advance().raw;
                        p.expect(Lexer::TokenKind::RSQUARE, "expected ']'");
                        arrayAlias += "]";
                    }
                    LOGX_DEBUG << "    parse_typedef: pointer-to-array -> " << arrayAlias;
                    p.match(Lexer::TokenKind::SEMICOLON);
                    decls.push_back(Lexer::Typedef(std::move(underlying), std::move(arrayAlias)));
                    return;
                }

                auto params = parse_param_list(p);
                LOGX_DEBUG << "    parse_typedef: function pointer -> " << alias;
                p.match(Lexer::TokenKind::SEMICOLON);
                decls.push_back(Lexer::FunctionPointer{std::move(underlying), std::move(alias), std::move(params)});
                return;
            }
        }
    }

    // Anonymous struct/union/enum with body: typedef struct { ... } Name;
    // Also handles: typedef struct tag { ... } Name;
    if ((underlying.baseType == "struct" || underlying.baseType == "union" ||
        underlying.baseType == "enum" ||
        underlying.baseType.find("struct ") == 0 ||
        underlying.baseType.find("union ") == 0 ||
        underlying.baseType.find("enum ") == 0) &&
        p.check(Lexer::TokenKind::LCURLY))
    {
        p.pos = saved;

        bool is_struct_or_union =
            underlying.baseType == "struct" || underlying.baseType == "union" ||
            underlying.baseType.find("struct ") == 0 ||
            underlying.baseType.find("union ") == 0;

        if (is_struct_or_union) {
            LOGX_DEBUG << "    parse_typedef: struct/union body -> typedef alias";
            decls.push_back(parse_struct_or_union(p));
            std::string alias;
            if (p.check(Lexer::TokenKind::IDENTIFIER))
                alias = p.advance().raw;
            LOGX_DEBUG << "    parse_typedef: alias = " << alias;
            p.match(Lexer::TokenKind::SEMICOLON);
            decls.push_back(Lexer::Typedef(
                Lexer::Type{underlying.baseType, 0, 0},
                std::move(alias)));
            return;
        }

        // Anonymous enum body
        LOGX_DEBUG << "    parse_typedef: anonymous enum body";
        p.advance(); // enum
        if (p.check(Lexer::TokenKind::IDENTIFIER))
            p.advance();
        p.expect(Lexer::TokenKind::LCURLY, "expected '{'");

        std::vector<Lexer::EnumField> fields;
        while (!p.at_end() && !p.check(Lexer::TokenKind::RCURLY)) {
            std::string field_name;
            if (p.check(Lexer::TokenKind::IDENTIFIER))
                field_name = p.advance().raw;
            std::optional<std::string> value;
            if (p.match(Lexer::TokenKind::EQUALS))
                value = parse_expression(p);
            fields.emplace_back(std::move(field_name), std::move(value));
            p.match(Lexer::TokenKind::COMMA);
        }
        p.expect(Lexer::TokenKind::RCURLY, "expected '}'");

        std::string enum_name;
        std::string alias;
        if (p.check(Lexer::TokenKind::IDENTIFIER))
            alias = p.advance().raw;
        LOGX_DEBUG << "    parse_typedef: enum alias = " << alias;
        p.match(Lexer::TokenKind::SEMICOLON);

        decls.push_back(Lexer::Enum(std::move(enum_name), std::move(fields)));
        decls.push_back(Lexer::Typedef(Lexer::Type{"enum", 0, 0}, std::move(alias)));
        return;
    }

    // Named struct/union/enum tag: typedef struct Tag Name;
    if (underlying.baseType == "struct" || underlying.baseType == "union" ||
        underlying.baseType == "enum" ||
        underlying.baseType.find("struct ") == 0 ||
        underlying.baseType.find("union ") == 0 ||
        underlying.baseType.find("enum ") == 0)
    {
        std::string alias;
        if (p.check(Lexer::TokenKind::IDENTIFIER))
            alias = p.advance().raw;
        LOGX_DEBUG << "    parse_typedef: named tag " << underlying.baseType << " -> " << alias;
        p.match(Lexer::TokenKind::SEMICOLON);
        decls.push_back(Lexer::Typedef(std::move(underlying), std::move(alias)));
        return;
    }

    // Regular typedef: typedef int MyInt;  (also: typedef int Matrix[4][4];)
    std::string alias;
    if (p.check(Lexer::TokenKind::IDENTIFIER))
        alias = p.advance().raw;
    alias += parse_array_dims(p);
    p.match(Lexer::TokenKind::SEMICOLON);
    decls.push_back(Lexer::Typedef(std::move(underlying), std::move(alias)));
}

// ---------------------------------------------------------------------------
// Variable / extern parsing
// ---------------------------------------------------------------------------

static Lexer::Declaration parse_variable(Parser& p, Lexer::Type type, std::string name) {
    LOGX_DEBUG << "      parse_variable: " << type.baseType << " " << name;

    // Array: type name[expr]
    // Previously this incremented pointerLevel and threw away the size,
    // turning e.g. "int lookup_table[16] = {...};" into the invalid
    // "int* lookup_table = {...};" (a brace-init list isn't a valid
    // initializer for a scalar pointer). Preserve the array declarator
    // on the name instead.
    name += parse_array_dims(p);

    // Optional initializer
    if (p.match(Lexer::TokenKind::EQUALS)) {
        std::string value = parse_expression(p);
        LOGX_DEBUG << "      parse_variable: initializer = " << value;
        p.match(Lexer::TokenKind::SEMICOLON);
        return Lexer::VariableDefinition(std::move(type), std::move(name), std::move(value));
    }

    p.match(Lexer::TokenKind::SEMICOLON);
    return Lexer::VariableDefinition(std::move(type), std::move(name));
}

// ---------------------------------------------------------------------------
// Function declaration / definition parsing
// ---------------------------------------------------------------------------

static Lexer::Declaration parse_function(Parser& p, Lexer::Type return_type, std::string name) {
    LOGX_DEBUG << "      parsing function: " << return_type.baseType << " " << name << "()";
    auto params = parse_param_list(p);
    LOGX_DEBUG << "      function has " << params.size() << " parameters";

    while (p.check_kw("const") || p.check_kw("volatile") ||
           p.check_kw("static") || p.check_kw("extern"))
        p.advance();

    if (p.check(Lexer::TokenKind::SEMICOLON)) {
        LOGX_DEBUG << "      function declaration (prototype)";
        p.advance();
        return Lexer::Function(std::move(name), std::move(params), std::move(return_type));
    }

    if (p.check(Lexer::TokenKind::LCURLY)) {
        LOGX_DEBUG << "      function definition, skipping body";
        // Skip function body
        int depth = 0;
        do {
            if (p.check(Lexer::TokenKind::LCURLY)) depth++;
            if (p.check(Lexer::TokenKind::RCURLY)) depth--;
            p.advance();
        } while (!p.at_end() && depth > 0);
        Lexer::Function fn(std::move(name), std::move(params), std::move(return_type));
        fn.has_body = true;
        return fn;
    }

    return Lexer::Function(std::move(name), std::move(params), std::move(return_type));
}

// ---------------------------------------------------------------------------
// Top-level statement parser
// ---------------------------------------------------------------------------

static void parse_top_level(Parser& p, std::vector<Lexer::Declaration>& decls) {
    if (p.at_end()) return;

    // Preprocessor directive
    if (p.check(Lexer::TokenKind::PREPROCESSOR)) {
        LOGX_DEBUG << "  parsing PREPROCESSOR at token " << p.pos;
        decls.push_back(parse_preprocessor(p));
        return;
    }

    // Forward declaration: struct Foo;
    if (looks_like_forward_decl(p)) {
        LOGX_DEBUG << "  parsing FORWARD_DECL at token " << p.pos;
        std::string keyword = p.advance().raw;
        std::string tag = p.advance().raw;
        p.advance(); // semicolon

        Lexer::ForwardKind fk = Lexer::ForwardKind::Struct;
        if (keyword == "union") fk = Lexer::ForwardKind::Union;
        else if (keyword == "enum") fk = Lexer::ForwardKind::Enum;

        decls.push_back(Lexer::ForwardDeclaration{fk, std::move(tag)});
        return;
    }

    // typedef
    if (p.check_kw("typedef")) {
        LOGX_DEBUG << "  parsing TYPEDEF at token " << p.pos;
        parse_typedef(p, decls);
        return;
    }

    // struct/union/enum definition (with body): struct Name { ... };
    if (p.check_kw("struct") || p.check_kw("union")) {
        size_t saved = p.pos;
        p.advance();
        skip_attribute(p);
        if (p.check(Lexer::TokenKind::IDENTIFIER))
            p.advance();
        skip_attribute(p);
        bool has_body = p.check(Lexer::TokenKind::LCURLY);
        p.pos = saved;
        if (has_body) {
            LOGX_DEBUG << "  parsing STRUCT/UNION definition at token " << p.pos;
            decls.push_back(parse_struct_or_union(p));
            return;
        }
    }

    if (p.check_kw("enum")) {
        size_t saved = p.pos;
        p.advance();
        if (p.check(Lexer::TokenKind::IDENTIFIER))
            p.advance();
        bool has_body = p.check(Lexer::TokenKind::LCURLY);
        p.pos = saved;
        if (has_body) {
            LOGX_DEBUG << "  parsing ENUM definition at token " << p.pos;
            decls.push_back(parse_enum(p));
            return;
        }
    }

    // Everything else: [qualifiers] type [*...] name ...
    size_t saved = p.pos;

    try {
        Lexer::Type type = parse_type(p);

        if (type.baseType.empty()) {
            LOGX_DEBUG << "  skipping unrecognized token at " << p.pos;
            p.skip_to_semicolon();
            return;
        }

        if (p.check(Lexer::TokenKind::IDENTIFIER)) {
            std::string name = p.advance().raw;

            if (type.flags & Lexer::Flags::isExtern && !p.check(Lexer::TokenKind::LPAREN)) {
                LOGX_DEBUG << "  parsing EXTERN_VARIABLE: " << name;
                name += parse_array_dims(p);
                p.match(Lexer::TokenKind::SEMICOLON);
                decls.push_back(Lexer::ExternVariable(std::move(type), std::move(name)));
                return;
            }

            if (p.check(Lexer::TokenKind::LPAREN)) {
                LOGX_DEBUG << "  parsing FUNCTION: " << name;
                decls.push_back(parse_function(p, std::move(type), std::move(name)));
                return;
            }

            LOGX_DEBUG << "  parsing VARIABLE: " << name;
            decls.push_back(parse_variable(p, std::move(type), std::move(name)));
            return;
        }

        p.pos = saved;
        p.skip_to_semicolon();
    } catch (...) {
        p.pos = saved;
        p.skip_to_semicolon();
    }
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

std::vector<Lexer::Declaration> parse(
    const std::vector<Lexer::Token>& tokens)
{
    LOGX_DEBUG << "parsing " << tokens.size() << " tokens";
    Parser p(tokens);
    std::vector<Lexer::Declaration> declarations;

    while (!p.at_end()) {
        if (p.match(Lexer::TokenKind::SEMICOLON))
            continue;

        size_t before_pos = p.pos;
        try {
            parse_top_level(p, declarations);
        } catch (const std::exception& e) {
            LOGX_ERROR << "parser error at token " << p.pos << ": " << e.what();
            std::cerr << "parser error: " << e.what() << std::endl;
            p.skip_to_semicolon();
        }
        // If the token position didn't move, we're stuck: force advance to avoid an
        // infinite loop. Comparing declaration count instead of position was wrong:
        // error-recovery paths (skip_to_semicolon, unrecognized-token skip, etc.) can
        // legitimately consume tokens without adding a declaration, and force-advancing
        // on top of that ate the first token of the next declaration, corrupting it.
        if (p.pos == before_pos && !p.at_end())
            p.advance();
    }

    LOGX_DEBUG << "parsing complete: " << declarations.size() << " declarations";
    return declarations;
}

std::vector<Lexer::DeclarationRange> parse_with_ranges(
    const std::vector<Lexer::Token>& tokens)
{
    LOGX_DEBUG << "parsing " << tokens.size() << " tokens (with ranges)";
    Parser p(tokens);
    std::vector<Lexer::Declaration> declarations;
    std::vector<Lexer::DeclarationRange> results;

    while (!p.at_end()) {
        if (p.match(Lexer::TokenKind::SEMICOLON))
            continue;

        size_t before_pos = p.pos;
        size_t start_line = tokens[before_pos].line_num;
        size_t end_line = start_line;
        size_t decl_before = declarations.size();
        try {
            parse_top_level(p, declarations);
        } catch (const std::exception& e) {
            LOGX_ERROR << "parser error at token " << p.pos << ": " << e.what();
            std::cerr << "parser error: " << e.what() << std::endl;
            p.skip_to_semicolon();
        }
        size_t decl_after = declarations.size();
        if (decl_after > decl_before) {
            if (p.pos > before_pos)
                end_line = tokens[p.pos - 1].line_num;
            for (size_t i = decl_before; i < decl_after; i++) {
                results.push_back({declarations[i], start_line, end_line});
            }
        }
        if (p.pos == before_pos && !p.at_end())
            p.advance();
    }

    LOGX_DEBUG << "parsing complete: " << results.size() << " declarations (with ranges)";
    return results;
}

// ---------------------------------------------------------------------------
// Declaration helpers
// ---------------------------------------------------------------------------

std::string declaration_kind_name(const Lexer::Declaration& decl) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Lexer::Function>)          return "Function";
        else if constexpr (std::is_same_v<T, Lexer::Struct>)       return "Struct";
        else if constexpr (std::is_same_v<T, Lexer::Union>)        return "Union";
        else if constexpr (std::is_same_v<T, Lexer::Enum>)         return "Enum";
        else if constexpr (std::is_same_v<T, Lexer::Typedef>)      return "Typedef";
        else if constexpr (std::is_same_v<T, Lexer::Macro>)        return "Macro";
        else if constexpr (std::is_same_v<T, Lexer::IncludeStatement>) return "Include";
        else if constexpr (std::is_same_v<T, Lexer::ExternVariable>)   return "ExternVariable";
        else if constexpr (std::is_same_v<T, Lexer::VariableDefinition>) return "Variable";
        else if constexpr (std::is_same_v<T, Lexer::ForwardDeclaration>) return "ForwardDecl";
        else if constexpr (std::is_same_v<T, Lexer::FunctionPointer>)  return "FunctionPointer";
        return "Unknown";
    }, decl);
}

static std::string flags_str(uint64_t flags) {
    std::string s;
    if (flags & Lexer::Flags::isStatic)   s += "static ";
    if (flags & Lexer::Flags::isExtern)   s += "extern ";
    if (flags & Lexer::Flags::isConst)    s += "const ";
    if (flags & Lexer::Flags::isVolitile) s += "volatile ";
    if (flags & Lexer::Flags::isSigned)   s += "signed ";
    if (flags & Lexer::Flags::isTypedef)  s += "typedef ";
    return s;
}

static void print_type(const Lexer::Type& t) {
    if (t.flags) std::cout << flags_str(t.flags);
    std::cout << t.baseType;
    for (uint32_t i = 0; i < t.pointerLevel; i++) std::cout << "*";
}

void print_declaration(const Lexer::Declaration& decl) {
    std::visit([](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Lexer::Function>) {
            print_type(arg.returnType);
            std::cout << " " << arg.name << "(";
            for (size_t i = 0; i < arg.params.size(); i++) {
                if (i > 0) std::cout << ", ";
                print_type(arg.params[i].dataType);
                if (arg.params[i].param_name.has_value())
                    std::cout << " " << *arg.params[i].param_name;
            }
            std::cout << ");" << std::endl;
        }
        else if constexpr (std::is_same_v<T, Lexer::Struct>) {
            std::cout << "struct " << arg.name << " {" << std::endl;
            for (const auto& f : arg.fields) {
                std::cout << "  ";
                print_type(f.type);
                std::cout << " " << f.name << ";" << std::endl;
            }
            std::cout << "};" << std::endl;
        }
        else if constexpr (std::is_same_v<T, Lexer::Union>) {
            std::cout << "union " << arg.name << " {" << std::endl;
            for (const auto& f : arg.fields) {
                std::cout << "  ";
                print_type(f.type);
                std::cout << " " << f.name << ";" << std::endl;
            }
            std::cout << "};" << std::endl;
        }
        else if constexpr (std::is_same_v<T, Lexer::Enum>) {
            std::cout << "enum " << arg.name << " {" << std::endl;
            for (const auto& f : arg.fields) {
                std::cout << "  " << f.name;
                if (f.value.has_value()) std::cout << " = " << *f.value;
                std::cout << "," << std::endl;
            }
            std::cout << "};" << std::endl;
        }
        else if constexpr (std::is_same_v<T, Lexer::Typedef>) {
            std::cout << "typedef ";
            print_type(arg.underlyingType);
            std::cout << " " << arg.alias << ";" << std::endl;
        }
        else if constexpr (std::is_same_v<T, Lexer::Macro>) {
            std::cout << "#define " << arg.name;
            if (arg.parameters.has_value()) std::cout << *arg.parameters;
            std::cout << " " << arg.body << std::endl;
        }
        else if constexpr (std::is_same_v<T, Lexer::IncludeStatement>) {
            std::cout << "#include " << (arg.system ? "<" : "\"")
                      << arg.path << (arg.system ? ">" : "\"") << std::endl;
        }
        else if constexpr (std::is_same_v<T, Lexer::ExternVariable>) {
            print_type(arg.type);
            std::cout << " " << arg.name << ";" << std::endl;
        }
        else if constexpr (std::is_same_v<T, Lexer::VariableDefinition>) {
            print_type(arg.dataType);
            std::cout << " " << arg.name;
            if (arg.value.has_value()) std::cout << " = " << *arg.value;
            std::cout << ";" << std::endl;
        }
        else if constexpr (std::is_same_v<T, Lexer::ForwardDeclaration>) {
            const char* kw = "struct";
            if (arg.kind == Lexer::ForwardKind::Union) kw = "union";
            else if (arg.kind == Lexer::ForwardKind::Enum) kw = "enum";
            std::cout << kw << " " << arg.name << ";" << std::endl;
        }
        else if constexpr (std::is_same_v<T, Lexer::FunctionPointer>) {
            print_type(arg.returnType);
            std::cout << " (*" << arg.name << ")(";
            for (size_t i = 0; i < arg.params.size(); i++) {
                if (i > 0) std::cout << ", ";
                print_type(arg.params[i].dataType);
                if (arg.params[i].param_name.has_value())
                    std::cout << " " << *arg.params[i].param_name;
            }
            std::cout << ");" << std::endl;
        }
    }, decl);
}

} // namespace Parser
