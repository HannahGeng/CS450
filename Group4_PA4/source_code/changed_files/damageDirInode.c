

#include "types.h"
#include "user.h"
#include "syscall.h"

int main(int argc, char *argv[]) {

  if (argc > 1) {
    printf(1, "print damaged node: %s \n", argv[1]);
  }
  int i = atoi(argv[1]);
  printf(1, "print damaged node: %d \n", i);
  damageDirInode(i);

  exit();
}
