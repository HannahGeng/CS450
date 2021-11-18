// mem_leak.c
#include <stdlib.h>

void test_memory_leak(int size) {
    malloc(size);
}

void no_leak() {
    void *p = malloc(1024 * 1024);
    free(p);
}

int main(int argc, char* argv[]) {
    no_leak();
    test_memory_leak(1024);

    return 0;
}
