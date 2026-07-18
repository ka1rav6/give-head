#include <stdio.h>
#include <stdbool.h>

extern int argc_global;
extern char** argv_global;
extern const char* const program_name;
extern volatile bool shutdown_requested;

extern int shared_memory[4096];
extern const double* lookup_ptr;
extern unsigned long long tick_counter;

void register_callback(int id, void (*cb)(void));
int execute(int argc, char** argv);
const char* get_version(void);
