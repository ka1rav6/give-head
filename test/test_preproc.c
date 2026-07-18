#include <stdio.h>
#include <stdlib.h>

#define CONCAT(a, b) a ## b
#define STRINGIFY(x) #x
#define PRINT_MSG(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) MIN(MAX(x, lo), hi)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE*)0)->MEMBER)

#define container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

#define SWAP(a, b, type) do { \
    type tmp = (a); \
    (a) = (b); \
    (b) = tmp; \
} while(0)

#define DEBUG_LOG(level, fmt, ...) \
    do { if (level <= DEBUG_LEVEL) fprintf(stderr, fmt, ##__VA_ARGS__); } while(0)

#define MAX_PATH 4096
#define MAX_ARGS 64
#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0
