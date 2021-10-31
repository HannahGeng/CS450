# CS 450 Operating Systems
Binghan Geng<br>
A20482350<br>
bgeng1@hawk.iit.edu

Yu Wang<br>
xxx<br>
xxx

# Part 1: Memory leaks and tools to find them

## Test case 1

```
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

```

## step 1: compile, and enable GDB debugging
![image](/screenshots/compile.png)

## step 2: set breakpoint at line 9
![image](/screenshots/gdb-1.png)

## step 3: executing line 9
Before executing to line 9, the total in use bytes (i.e. the memory being used by the process) is 0. After executing line 9, the total in use bytes is 1053264, at which point free.1053264 is similar to the space allocated using malloc 1024 * 1024 = 1048576 (part of the space is used for 1053264 is similar to the space allocated using malloc, 1024 * 1024 = 1048576 (part of the space is used to maintain the memory allocation data structure). 

![image](/screenshots/gdb-3.png)

## step 4: executing the free() statement
After executing the free() statement, you can see that the in use bytes are reduced to 592. malloc() frees the space obtained.

![image](/screenshots/gdb-2.png)

## step 5:
Next, execute the test_memory_leak() function, we can observe that before the execution of the function, the in use bytes is 592, after the execution is 1632, a total of 1040bytes of space is still in use and not freed, there is a memory leak.

![image](/screenshots/gdb-4.png)

Therefore, with the help of the gdb tool, it is possible to detect memory leaks by observing the change in the value of in use bytes before and after function execution.

![image](/screenshots/mem_leak.png)

## Test case 2
```
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
```

The rationale for this test case is that the design of a program that points a pointer to a new space and loses ownership of the originally allocated space is one of the common, and often insidious, causes of memory leaks.

The expected result is that the initial allocation of 4096 bytes is not reclaimed.

![image](/screenshots/mem_leak2.png)
