#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1,"process pid:%d \n", getpid());
  printf(1, "start read shared memory\n");

  int key,num_pages, r;

  if(argc <= 1){
    key = 1;
    num_pages = 3;
  }

  key = atoi(argv[1]);
  num_pages = atoi(argv[1]);
  printf(1, "param: key:%d, num_pages: dx \n", key, num_pages);
  char *pa = (char*)GetSharedPage(key,num_pages);

  printf(1, "GetSharedPage Return: key:%d, address: %x \n", key, (unsigned int)pa);
  printf(1, "read [%s] from key[%d]-[%x] \n", pa, key, (unsigned int)pa);

  printf(1, "start release shared memory\n");
  r = FreeSharedPage(key);
  if (r == -1){
    printf(1, "not shared memory free\n");
  }else {
    printf(1, "free shared memory: key[%d]-[%x] \n", key, (unsigned int) pa);
  }

  exit();

}