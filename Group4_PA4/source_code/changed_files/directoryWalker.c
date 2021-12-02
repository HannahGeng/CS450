
#include "types.h"
#include "user.h"
#include "syscall.h"

int main(int argc, char *argv[]) {
	//(argc <= 1) ? directoryWalker(".") : directoryWalker(argv[1]);

    if (argc <= 1){
      printf(1, "print path .\n");
      directoryWalker(".");
    } else {
      printf(1, "print path: %s \n", argv[1]);
      directoryWalker(argv[1]);
    }

	exit();
}
