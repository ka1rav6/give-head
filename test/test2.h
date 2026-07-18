/* This header is generated using copa 
*  (https://github.com/ka1rav6/copa)
*/

#ifndef TEST2_H
#define TEST2_H

#include <stdio.h>

#include <stdlib.h>

#include "myheader.h"

#include <stdbool.h>

#define PI 3.14159

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define EMPTY 

typedef unsigned long ulong;

typedef const char* string;

typedef int (*handler_t)(void*, int);

struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point top_left;
    struct Point bottom_right;
};

union Data {
    int i;
    float f;
    char* s;
};

enum Color {
    RED = 0,
    GREEN = 1,
    BLUE = 2,
    YELLOW
};

typedef struct {
    double r;
    double theta;
} Polar;
typedef enum {
    MON = 1,
    TUE,
    WED,
    THU,
    FRI,
    SAT,
    SUN
} Weekday;
typedef void (*callback_t)(int, const char*);

typedef int (*compare_fn)(const void*, const void*);

struct Forward;

union Opaque;

extern int global_count;

extern const char* prog_name;

static int helper(int x);

int public_api(int a, int b);

const char* get_name();

int* get_array(int size);

char** get_strings(int count);

int add(int a, int b);

void process(const char* data, int len);

#endif /* TEST2_H */
