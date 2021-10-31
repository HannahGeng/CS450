// mem_leak2.c
#include <stdlib.h>

void test_memory_leak() {
    void *p = malloc(4096);
    p = malloc(1024);
    free(p);
}

int main(int argc, char* argv[]) {
    test_memory_leak();

    return 0;
}
