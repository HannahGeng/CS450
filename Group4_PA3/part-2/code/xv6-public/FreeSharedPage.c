#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  printf(1,"process pid:%d \n", getpid());
  printf(1, "start read shared memory\n");

  int key,num_pages, r;

  key = 1;
  num_pages = 3;

  char *pa = (char*)GetSharedPage(key,num_pages);
  printf(1, "return: key:%d, address: %x \n", key, (unsigned int)pa);

  //printf(1, "read [%s] from key[%d]-[%x] \n", pa, key, (unsigned int)pa);

  printf(1, "start release shared memory\n");
  r = FreeSharedPage(key);
  if (r == -1){
    printf(1, "not shared memory free\n");
  }else {
    printf(1, "free shared memory: key[%d]-[%x] \n", key, (unsigned int) pa);
  }

  exit();

}