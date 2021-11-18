#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  printf(1,"process pid:%d \n", getpid());
  printf(1, "start write shared memory\n");

  int key, num_pages;
  key = 1;
  num_pages = 3;
  char *pa = (char*)GetSharedPage(key,num_pages);
  printf(1,"return: key:%d, address: %x \n", key, (unsigned int)pa);
  strcpy(pa, "Hello,CS450 PA3!");
  printf(1, "write [%s]into key[%d]-[%x] \n", pa, key, (unsigned int)pa);

  exit();

}