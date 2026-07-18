# ROADMAP

## Phase 1:
- [x] Basic Lexer for C programs
- [x] A proper lexer of C programs
- [x] A file system reader that reads C files

## Phase 2:
- [x] C parser: type parsing (qualifiers, pointers, struct/union/enum tags)
- [x] C parser: function declarations and definitions
- [x] C parser: struct, union, enum definitions with fields
- [x] C parser: typedef (scalar, struct, enum, function pointers)
- [x] C parser: preprocessor directives (#include, #define)
- [x] C parser: extern variables and forward declarations
- [x] C parser: edge cases (bitfields, variadics, nested anonymous types, multi-line)


## Phase 3:
- [x] Header file generator from AST
- [x] Include guards
- [x] Deduplicate function declarations (prototype + definition)
- [x] Combine anonymous typedef struct/union/enum
- [x] CLI: single file, directory, and recursive (-r) modes
- [x] Generate headers in same directory as source

## COOL FEATURES PLANNED
- [ ] pragma once
- [ ] preserve comments
- [ ] generate documentation
- [ ] detect missing prototypes
- [ ] sort declarations alphabetically
- [ ] output dependency graph
- [ ] generate UML
- [ ] color diagnostics
- [ ] watch mode
- [ ] parallel parsing
