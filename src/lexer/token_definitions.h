#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <cstdint>
#include <variant>
#include <optional>
#include <string>
#include <vector>


namespace Lexer{

// bitmasked flags for variables and functions
enum Flags : uint8_t {
    isStatic   = 1 << 0,
    isExtern   = 1 << 1,
    isTypedef  = 1 << 2,
    isVolitile = 1 << 3,
    isSigned   = 1 << 4,
    isConst    = 1 << 5,
};

struct Type {
    std::string baseType;
    uint32_t pointerLevel = 0;
    uint64_t flags        = 0;
};

struct Parameter {
    Type dataType;
    std::optional<std::string> param_name;

    explicit Parameter(
        Type type,
        std::optional<std::string> name = std::nullopt)
        : dataType(std::move(type)),
          param_name(std::move(name)) {}
};

struct Function {
    std::string name;
    std::vector<Parameter> params;
    Type returnType;
    uint8_t flags = 0;

    Function(
        std::string name,
        std::vector<Parameter> params,
        Type returnType)
        : name(std::move(name)),
          params(std::move(params)),
          returnType(std::move(returnType)) {}
};


struct EnumField {
    std::string name;
    std::optional<std::string> value;

    explicit EnumField(
        std::string name,
        std::optional<std::string> value = std::nullopt)
        : name(std::move(name)),
          value(std::move(value)) {}
};

struct Enum {
    std::string name;
    std::vector<EnumField> fields;
    Enum(
        std::string name,
        std::vector<EnumField> fields)
        : name(std::move(name)),
          fields(std::move(fields)) {}
};

struct ExternVariable {
    Type type;
    std::string name;

    ExternVariable(
        Type type,
        std::string name)
        : type(std::move(type)),
          name(std::move(name)) {}
};

struct StructField {
    Type type;
    std::string name;
    StructField(
        Type type,
        std::string name)
        : type(std::move(type)),
          name(std::move(name)) {}
};

struct Struct {
    std::string name;
    std::vector<StructField> fields;

    Struct(
        std::string name,
        std::vector<StructField> fields)
        : name(std::move(name)),
          fields(std::move(fields)) {}
};

struct VariableDefinition {
    Type dataType;
    std::string name;
    std::optional<std::string> value;
    VariableDefinition(
        Type dataType,
        std::string name,
        std::optional<std::string> value = std::nullopt)
        : dataType(std::move(dataType)),
          name(std::move(name)),
          value(std::move(value)) {}
};

struct Typedef{
    Type underlyingType;
    std::string alias;
    
    Typedef(   Type underlyingType,
               std::string alias
            ): underlyingType(std::move(underlyingType)),
               alias(std::move(alias)){}
};
struct UnionField{
    std::string name;
    Type type;
    UnionField(  std::string name,
                 Type type
              ): name(std::move(name)),
                 type(std::move(type))
    {}
};

struct Union{
    std::string name;
    std::vector<UnionField> fields;
    Union(  std::string name,
            std::vector<UnionField> fields
         ): name(std::move(name)),
            fields(std::move(fields)){}
};

struct Macro {
    std::string name;
    std::optional<std::string> parameters;
    std::string body;

    Macro(
            std::string name,
            std::string body,
            std::optional<std::string> parameters = std::nullopt
        ) : name(std::move(name)),
            body(std::move(body)),
            parameters(std::move(parameters))
    {}
};

struct IncludeStatement{
    std::string path;
    bool system;
    IncludeStatement(
            bool system,
            std::string path
            ) : system(system),
                path(std::move(path))
    {}
};

enum class ForwardKind {
    Struct,
    Union ,
    Enum
};

struct ForwardDeclaration {
    ForwardKind kind;
    std::string name;
};

struct FunctionPointer {
    Type returnType;
    std::string name;
    std::vector<Parameter> params;
};

struct ArrayInfo {
    Type elementType;
    std::string name;
    std::optional<size_t> size;
};

struct Comment {
    std::string text;
};


using Declaration =
    std::variant<
        Function          ,
        Struct            ,
        Union             ,
        Enum              ,
        Typedef           ,
        Macro             ,
        IncludeStatement  ,
        ExternVariable    ,
        VariableDefinition,
        ForwardDeclaration
    >;

// ------ Punctuation / Operators ------
enum class Symbols : uint8_t {
    // ------- Brackets -------
    LPAREN,                     // (
    RPAREN,                     // )
    LSQUARE,                    // [
    RSQUARE,                    // ]
    LCURLY,                     // {
    RCURLY,                     // }
    // ------- Comments --------
    START_MULTICOMMENT,         // /*
    END_MULTICOMMENT,           // */
    SINGLE_COMMENT,             // //
    // ------ Arithmetic --------
    PLUS,                       // +
    PLUSPLUS,                   // ++
    MINUS,                      // -
    MINUSMINUS,                 // --
    ASTRISK,                    // *
    DIVIDE,                     // /
    EQ,                         // =
    EQ_EQ,                      // ==
    NE,                         // !=
    LT,                         // <
    GT,                         // >
    GE,                         // >=
    LE,                         // <=
    AND,                        // &
    ANDAND,                     // &&
    OR,                         // |
    OROR,                       // ||
    LOG_NOT,                    // !
    BIN_NOT,                    // ~
    PTR_NOTATION,               // ->
    // ------ Punctuation -------
    SINGLE_QUOTE,               // '
    DOUBLE_QUOTE,               // "
    COMMA,                      // ,
    QSN_MARK,                   // ?
    NEXT_LINE,                  // \n
    EOF_,                       // EOF
    SEMICOLON,                  // ;
    COLON,                      // :
    DOT,                        // .
    ELLIPSIS,                   // ...
    HASH,                       // #
    AT,                         // @
};

// ------ Token Kinds ------
enum class TokenKind : uint8_t {
    // literals
    IDENTIFIER,
    KEYWORD,
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    CHAR_LITERAL,
    HEX_LITERAL,
    OCTAL_LITERAL,

    // single character tokens
    LPAREN,                     // (
    RPAREN,                     // )
    LSQUARE,                    // [
    RSQUARE,                    // ]
    LCURLY,                     // {
    RCURLY,                     // }
    SEMICOLON,                  // ;
    COLON,                      // :
    COMMA,                      // ,
    DOT,                        // .
    TILDE,                      // ~
    QUESTION,                   // ?

    // operators
    PLUS,                       // +
    MINUS,                      // -
    STAR,                       // *
    SLASH,                      // /
    PERCENT,                    // %
    AMPERSAND,                  // &
    PIPE,                       // |
    CARET,                      // ^
    BANG,                       // !
    EQUALS,                     // =

    // comparison
    LT,                         // <
    GT,                         // >

    // two character operators
    PLUS_PLUS,                  // ++
    MINUS_MINUS,                // --
    ARROW,                      // ->
    EQ_EQ,                      // ==
    BANG_EQ,                    // !=
    LT_EQ,                      // <=
    GT_EQ,                      // >=
    AMP_AMP,                    // &&
    PIPE_PIPE,                  // ||
    LT_LT,                      // <<
    GT_GT,                      // >>
    AMP_EQ,                     // &=
    PIPE_EQ,                    // |=
    CARET_EQ,                   // ^=
    PLUS_EQ,                    // +=
    MINUS_EQ,                   // -=
    STAR_EQ,                    // *=
    SLASH_EQ,                   // /=
    PERCENT_EQ,                 // %=

    // three character operators
    ELLIPSIS,                   // ...

    // preprocessor
    PREPROCESSOR,               // #include, #define, etc

    // comments
    LINE_COMMENT,               // // ...
    BLOCK_COMMENT,              // /* ... */

    // misc
    NEWLINE,
    UNKNOWN,
};

struct Token {
    TokenKind kind;
    std::string raw;
    size_t col_num ;
    size_t line_num;
};

} // namespace Lexer

#endif
