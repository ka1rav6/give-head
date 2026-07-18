#include <stdio.h>

typedef int Matrix[4][4];
typedef char Buffer[256];
typedef double (*Grid)[10];

int lookup_table[16] = {
    0, 1, 1, 2, 3, 5, 8, 13,
    21, 34, 55, 89, 144, 233, 377, 610
};

extern int shared_buffer[1024];
extern const char* argv_cache[64];

void process_grid(double grid[8][8]);
void swap_rows(int rows[][3], int a, int b);
