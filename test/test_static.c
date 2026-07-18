static int counter = 0;
static const char* NAME = "copa";
static volatile int flag;
static unsigned long table[256];

static int helper(int x) {
    return x * 2;
}

static void init_array(int arr[], int size) {
    for (int i = 0; i < size; i++)
        arr[i] = i;
}
