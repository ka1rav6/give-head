/* This header is generated using copa 
*  (https://github.com/ka1rav6/copa)
*/

#ifndef TEST_FUNCPTR_H
#define TEST_FUNCPTR_H

#include <stdbool.h>

#include <stdint.h>

typedef void (*signal_handler_t)(int);

typedef int (*command_fn)(int argc, char** argv);

typedef void (*draw_fn)(void* surface, int x, int y);

struct  {
    const char* name;
    command_fn execute;
    const char* help;
};

#endif /* TEST_FUNCPTR_H */
