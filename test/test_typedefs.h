/* This header is generated using copa 
*  (https://github.com/ka1rav6/copa)
*/

#ifndef TEST_TYPEDEFS_H
#define TEST_TYPEDEFS_H

#include <stddef.h>

#include <stdint.h>

typedef uint8_t byte;

typedef uint16_t word;

typedef uint32_t dword;

typedef int (*sort_fn)(const void*, const void*);

typedef void (*cleanup_fn)(void*);

typedef struct {
    sort_fn compare;
    cleanup_fn destroy;
    void* context;
} SortConfig;
typedef enum {
    STATE_IDLE = 0,
    STATE_RUNNING = 1,
    STATE_PAUSED = 2,
    STATE_ERROR = - 1
} TaskState;
struct  {
    const byte* data;
    size_t length;
    word checksum;
};

struct  {
    const char* key;
    const char* value;
    size_t value_len;
};

#endif /* TEST_TYPEDEFS_H */
