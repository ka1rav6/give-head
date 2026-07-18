#include <stdbool.h>
#include <stdint.h>

typedef void (*signal_handler_t)(int);
typedef int (*command_fn)(int argc, char** argv);
typedef void (*draw_fn)(void* surface, int x, int y);

signal_handler_t signal_table[16];
draw_fn renderers[256];

command_fn get_command(const char* name);
void register_signal(int sig, signal_handler_t handler);
void dispatch(int id, command_fn commands[], int argc, char** argv);

typedef struct {
    const char* name;
    command_fn execute;
    const char* help;
} CommandEntry;

typedef struct {
    const char* event_name;
    void (*on_event)(void* data);
    void* user_data;
} EventBinding;
