#include <stddef.h>
#include <stdint.h>

struct __attribute__((packed)) PackedHeader {
    uint8_t version;
    uint32_t length;
    uint16_t flags;
};

typedef struct {
    volatile int ref_count;
    const char* type_name;
    size_t size;
} ObjectHeader;

struct TreeNode {
    int value;
    struct TreeNode* left;
    struct TreeNode* right;
    struct TreeNode* parent;
};

typedef struct {
    int fd;
    int flags;
    struct {
        size_t read_pos;
        size_t write_pos;
        size_t capacity;
    } buffer;
    const struct {
        const char* name;
        int (*validate)(int fd);
    }* ops;
} FileDescriptor;

union AlignedData {
    int i;
    float f;
    void* ptr;
    long long big[8];
};

extern const struct TreeNode NIL_NODE;
extern const ObjectHeader base_object;
