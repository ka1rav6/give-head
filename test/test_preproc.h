/* This header is generated using copa 
*  (https://github.com/ka1rav6/copa)
*/

#ifndef TEST_PREPROC_H
#define TEST_PREPROC_H

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

#endif /* TEST_PREPROC_H */
