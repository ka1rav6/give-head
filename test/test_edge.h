/* This header is generated using copa 
*  (https://github.com/ka1rav6/copa)
*/

#ifndef TEST_EDGE_H
#define TEST_EDGE_H

#include <stdarg.h>

struct Bitfields {
    unsigned int flag_a;
    unsigned int flag_b;
    unsigned int value;
};

int variadic(int count);

struct node {
    int data;
    struct node* next;
};

typedef struct node Node;

struct Container {
    struct inner;
    int z;
};

#endif /* TEST_EDGE_H */
