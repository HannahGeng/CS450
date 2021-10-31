# CS450 Operating System
Binghan Geng<br>
A20482350<br>
bgeng1@hawk.iit.edu

# Part 1

### Introduction
Memory leaks and tools to find them.

### Build & Run GDB
![image](/screenshots/compile.png)

### Compile instruction:

`gcc mem_leak.c -g -o mem_leak
gdb ./mem_leak`

Valgrind instruction:

`valgrind --leak-check=yes ./mem_leak`
