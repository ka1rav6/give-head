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
    STATE_ERROR = -1
} TaskState;

typedef struct {
    TaskState state;
    int priority;
    int (*execute)(void* task);
    void* data;
} Task;

typedef struct {
    const byte* data;
    size_t length;
    word checksum;
} Packet;

typedef struct {
    const char* key;
    const char* value;
    size_t value_len;
} ConfigEntry;
