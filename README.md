# copa

A C header file generator that parses `.c` source files and generates clean, ready-to-use `.h` header files with include guards.

**Version: 0.1.1**

## Example

Given this C source file:

```c
// foo.c
#include <stdio.h>

#define MAX_SIZE 1024
#define SQUARE(x) ((x) * (x))

typedef struct {
    int x;
    int y;
} Point;

typedef void (*callback_t)(int, const char*);

extern int global_var;

int add(int a, int b);
int* get_pointer(const char* name);
```

Running `copa foo.c` produces `foo.h`:

```c
/* This header is generated using copa
*  (https://github.com/ka1rav6/copa)
*/

#ifndef FOO_H
#define FOO_H

#include <stdio.h>

#define MAX_SIZE 1024
#define SQUARE(x) ((x) * (x))

typedef struct {
    int x;
    int y;
} Point;

typedef void (*callback_t)(int, const char*);

extern int global_var;

int add(int a, int b);

int* get_pointer(const char* name);

#endif /* FOO_H */
```

Function bodies are stripped to prototypes. Duplicates (prototype + definition of the same function) are deduplicated. Anonymous `typedef struct { ... } Name;` patterns are correctly combined.

## How it works

copa processes a `.c` file through a 4-stage pipeline:

1. **Lexer** -- Tokenizes the source into a stream of tokens (identifiers, keywords, operators, literals, preprocessor directives, etc.)
2. **Flattener** -- Merges per-line token vectors into a single stream, stripping comments and newlines.
3. **Parser** -- Parses tokens into an AST of top-level declarations (functions, structs, unions, enums, typedefs, macros, includes, extern variables, forward declarations, function pointers).
4. **Generator** -- Walks the AST and emits a `.h` header file with include guards and all declarations converted to header-appropriate form.

## Building

Requires CMake 3.16+ and a C++20 compiler.

```bash
cmake -B build -S .
cmake --build build
```

The executable is at `build/src/copa`.

### Release build (no debug logs)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Usage

```
copa [-r] [--watch] <file.c | directory>
```

| Command | Description |
|---------|-------------|
| `copa src/foo.c` | Generate `src/foo.h` from `src/foo.c` |
| `copa src/` | Generate headers for all `.c` files in `src/` |
| `copa -r src/` | Generate headers for all `.c` files in `src/` and subdirectories |
| `copa --watch src/` | Watch `src/` and regenerate headers when `.c` files change |
| `copa -r --watch src/` | Watch `src/` and subdirectories recursively |

- Headers are generated in the **same directory** as the source file.
- Non-`.c` files are automatically skipped.
- Existing headers are overwritten.

### Examples

```bash
# Single file
./build/src/copa src/main.c          # produces src/main.h

# All .c files in a directory
./build/src/copa src/                # produces src/*.h

# Recursive
./build/src/copa -r project/         # all .c files in project/ and subdirs

# Watch mode -- regenerates headers on file save
./build/src/copa --watch src/        # watch src/, regenerate on change
./build/src/copa -r --watch .        # watch entire project recursively
```

## What's implemented

### Lexer (`src/lexer/`)
- Full C tokenization: identifiers, keywords, all operators (single/double/triple char), number literals (int, float, hex, octal, with type suffixes), string/char literals with escape handling
- Preprocessor directives (consumed as single tokens)
- Single-line (`//`) and block (`/* */`) comments

### Parser (`src/parser/`)
- Type parsing with qualifiers (`const`, `static`, `extern`, `volatile`, `signed`), pointer levels, struct/union/enum tags
- Function declarations and definitions (parameters, variadics, `(void)` handling)
- Struct, union, enum definitions with named and anonymous fields
- Typedefs: scalar, struct, enum, function pointers
- Preprocessor directives: `#include` (system `<...>` and local `"..."`), `#define` (simple and function-like)
- Extern variables and forward declarations
- Multi-line declarations
- Error recovery (skips to next semicolon on parse failure)

### Header Generator (`src/generator/`)
- Include guards derived from output filename
- All relevant declarations emitted for a header:
  - `#include` directives (system and local)
  - `#define` macros (simple and function-like)
  - Struct, union, enum definitions with full field bodies
  - Anonymous `typedef struct/union/enum { ... } Name;` combined into single declarations
  - Function pointer typedefs
  - Extern variable declarations
  - Function prototypes (bodies stripped, duplicates deduplicated)
  - Forward declarations (`struct Foo;`, etc.)

### Watch mode
- Polls directory every 500ms for `.c` file changes
- Regenerates headers automatically on modification
- Supports recursive watching with `-r`

## Test files

The `test/` directory contains C source files and their corresponding generated headers:

| Source | Contents |
|--------|----------|
| `test/test.c` | Includes, macros, typedef struct/enum, function pointer, extern, prototypes, forward declarations |
| `test/test2.c` | Multiple includes, named structs/unions/enums, multiple typedefs, extern with qualifiers |
| `test/test_edge.c` | Bitfields, variadics, self-referential structs, anonymous struct/union members |
| `test/test_multiline.c` | Multi-line function signatures and struct definitions |
| `test/test_selfref.c` | Self-referential struct typedef (linked list node) |
| `test/test_static.c` | `static` variables, `const`, `volatile`, `static` functions |
| `test/test_arrays.c` | Multi-dimensional arrays, typedef'd arrays, array initializers |
| `test/test_funcptr.c` | Function pointer typedefs, function pointer arrays |
| `test/test_structs.c` | Packed attributes, tree nodes, deeply nested structs |
| `test/test_preproc.c` | Token concatenation, stringification, variadic macros, `container_of` |
| `test/test_typedefs.c` | Typedef chains, typedef'd enums, multi-field typedef'd structs |
| `test/test_globals.c` | Extern with qualifiers, pointer-returning externs |

Run all tests:

```bash
./build/src/copa -r test/
```

## License

BSD 2-Clause License. See [LICENSE](LICENSE).
