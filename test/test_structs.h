/* This header is generated using copa 
*  (https://github.com/ka1rav6/copa)
*/

#ifndef TEST_STRUCTS_H
#define TEST_STRUCTS_H

#include <stddef.h>

#include <stdint.h>

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
    struct buffer;
    const struct ;
    * ops;
} FileDescriptor;
extern const struct TreeNode NIL_NODE;

extern const ObjectHeader base_object;

#endif /* TEST_STRUCTS_H */
