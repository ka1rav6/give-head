/* This header is generated using copa 
*  (https://github.com/ka1rav6/copa)
*/

#ifndef TEST_GLOBALS_H
#define TEST_GLOBALS_H

#include <stdio.h>

#include <stdbool.h>

extern int argc_global;

extern char** argv_global;

volatile bool shutdown_requested;

extern int shared_memory;

const double* lookup_ptr;

extern unsigned long long tick_counter;

char* get_version();

#endif /* TEST_GLOBALS_H */
