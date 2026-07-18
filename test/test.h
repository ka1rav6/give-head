/* This header is generated using copa 
*  (https://github.com/ka1rav6/copa)
*/

#ifndef TEST_H
#define TEST_H

#include <stdio.h>

#include "myheader.h"

#define MAX_SIZE 1024

#define SQUARE(x) ((x) * (x))

typedef struct {
    int x;
    int y;
} Point;
typedef enum {
    RED,
    GREEN,
    BLUE
} Color;
typedef void (*callback_t)(int, const char*);

extern int global_var;

int add(int a, int b);

static void internal_func();

int* get_pointer(const char* name);

struct Forward;

int main(int argc, char** argv);

#endif /* TEST_H */
