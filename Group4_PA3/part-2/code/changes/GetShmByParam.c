#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1,"process pid:%d \n", getpid());
  printf(1, "start write shared memory\n");

  int key,num_pages;
  if(argc <= 1){
    key = 1;
    num_pages = 3;
  }

  key = atoi(argv[1]);
  num_pages = atoi(argv[1]);

  printf(1, "param: key:%d, num_pages: dx \n", key, num_pages);

  char *pa = (char*)GetSharedPage(key,num_pages);
  printf(1,"return: key:%d, address: %x \n", key, (unsigned int)pa);
  strcpy(pa, "Hello,CS450 PA3!");
  printf(1, "write [%s]into key[%d]-[%x] \n", pa, key, (unsigned int)pa);

  exit();

}