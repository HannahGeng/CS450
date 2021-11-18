#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "spinlock.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

#define MAX_SHM_PGNUM (4) 	//Up to 4 pages of memory per shared memory

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpunum()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);

  // Map cpu and proc -- these are private per cpu.
  c->gdt[SEG_KCPU] = SEG(STA_W, &c->cpu, 8, 0);

  lgdt(c->gdt, sizeof(c->gdt));
  loadgs(SEG_KCPU << 3);

  // Initialize cpu-local storage.
  cpu = c;
  proc = 0;
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0)
      return 0;
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  pushcli();
  cpu->gdt[SEG_TSS] = SEG16(STS_T32A, &cpu->ts, sizeof(cpu->ts)-1, 0);
  cpu->gdt[SEG_TSS].s = 0;
  cpu->ts.ss0 = SEG_KDATA << 3;
  cpu->ts.esp0 = (uint)proc->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  cpu->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= proc->top)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a += (NPTENTRIES - 1) * PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, proc->top, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0)
      goto bad;
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

struct sharemempage
{
    int refcount;     		//Reference count for each key
    void* physaddr[MAX_SHM_PGNUM];   	//Corresponding to the physical address of per page
};

struct spinlock shmlock;    			    //Locks for exclusive access
struct sharemempage shmarray[NKEY];  		//Array of Share memory page record
int page_nums[NKEY];    // Number of physical pages corresponding to the key


int
mapshmex(pde_t *pgdir, uint oldva, uint newva, uint sz, void **physaddr)
{
  uint a;
  if(oldva > KERNBASE || newva < sz)
    return 0;  												//Validation Parameters
  a=newva;
  for (int i = 0;a<oldva;a+=PGSIZE, i++) 					//Page-by-Page Mapping
  {
    mappages(pgdir,(char*)a,PGSIZE,(uint)physaddr[i],PTE_W|PTE_U);
  }
  return newva;
}

// This method is basically the same as the deallocuvm implementation,the difference is
// that our memory usage is dealloced from low addresses to high addresses.
int
deallocshmex(pde_t *pgdir, uint oldva, uint newva)
{
  pte_t *pte;
  uint a, pa;
  if(newva <= oldva)
    return oldva;
  a = (uint)PGROUNDDOWN(newva - PGSIZE); //
  for (; oldva <= a; a-=PGSIZE)
  {
    pte = walkpgdir(pgdir,(char*)a,0);
    if(pte && (*pte & PTE_P)!=0){
      pa = PTE_ADDR(*pte);
      if(pa == 0){
        panic("kfree");
      }
      *pte = 0;
    }
  }
  return newva;
}

// This method is basically the same as the allcouvm implementation, the difference is
// that our memory usage is allocated from high addresses to low addresses.
int
allocshmex(pde_t *pgdir, uint oldva, uint newva, uint sz,void *phyaddr[MAX_SHM_PGNUM])
{
  char *mem;
  uint a;

  if(oldva > KERNBASE || newva < sz)
    return 0;
  a = newva;
  for (int i = 0; a < oldva; a+=PGSIZE, i++)
  {
    mem = kalloc(); 		//Assigning physical page frames
    if(mem == 0){
      // cprintf("allocshm out of memory\n");
      deallocshmex(pgdir,newva,oldva);
      return 0;
    }
    memset(mem,0,PGSIZE);
    mappages(pgdir,(char*)a,PGSIZE,(uint)V2P(mem),PTE_W|PTE_U);	//页表映射
    phyaddr[i] = (void *)V2P(mem);
    // cprintf("allocshm : %x\n",a);
  }
  return newva;
}

//initialize the shared variable array shmarray[].
void
shminit()
{
  initlock(&shmlock,"shmplock");   //Initializing locks

  for (int i = 0; i < NKEY; i++)     //Initialize shmarray
  {
    shmarray[i].refcount = 0;
    page_nums[i] = 0;
    for(int j = 0; j < MAX_SHM_PGNUM; j++) {
      shmarray[i].physaddr[j] = 0;
    }
  }

}

// Fill the new memory area information into the shmarray[8] array
int
shmadd(uint key, uint pagenum, void* physaddr[MAX_SHM_PGNUM])
{
  if(key<0 || NKEY<=key || pagenum<0 || MAX_SHM_PGNUM < pagenum){
    return -1;
  }
  shmarray[key].refcount = 1;
  page_nums[key] = pagenum;

  for(int i = 0;i<pagenum;++i){
    shmarray[key].physaddr[i] = physaddr[i];
  }
  return 0;
}

// maps the shared memory pages area to the process space.
// key:  a subscript of the shared memory pages array.
// num_pages: the number of pages to be allocated, and returns the address of the shared memory.
void *
GetSharedPage(uint key, uint num_pages) {
  void *phyaddr[MAX_SHM_PGNUM];
  uint addr = 0;
  if (key < 0 || NKEY <= key || num_pages < 0 || MAX_SHM_PGNUM < num_pages)    //Calibration parameters
    return (void *) -1;
  addr = proc->top;
  // If the current process has already mapped the shared memory of the key, return the address directly.
  // Do not support multiple mappings of the same shared memory area in the same process.
  if (proc->shm_keys[key] == 1) {
    return proc->shm_va[key];
  }
  acquire(&shmlock);
  // If the system has not yet created the shared memory corresponding to this key, then allocate the memory and map.
  if (shmarray[key].refcount == 0) {
    addr = allocshmex(proc->pgdir, addr, addr - num_pages * PGSIZE, proc->sz, phyaddr);
    //The new allocshmex() allocates memory and maps it, the same principle as allcouvm()
    if (addr == 0) {
      release(&shmlock);
      return (void *) -1;
    }
    proc->shm_va[key] = (void *) addr;
    shmadd(key, num_pages, phyaddr);    //Fill the new memory area information into the shmarray[8] array
  } else {
    //If the key is not held and the shared memory corresponding to this key is already allocated in the system,
    // then it is mapped directly.
    for (int i = 0; i < num_pages; i++) {
      phyaddr[i] = shmarray[key].physaddr[i];
    }
    num_pages = page_nums[key];
    //The mapshmex method creates a new mapping
    if ((addr = mapshmex(proc->pgdir, addr, addr - num_pages * PGSIZE, proc->sz, phyaddr)) == 0) {
      release(&shmlock);
      return (void *) -1;
    }
    proc->shm_va[key] = (void *) addr;
    shmarray[key].refcount++;            //Reference Count +1
  }
  proc->top = addr;
  proc->shm_keys[key] = 1;
  release(&shmlock);
  return (void *) addr;
}

// release/unmap the shared memory pages, and is also called to release the physical memory
// to completely destroy the shared memory.
int
FreeSharedPage(uint key) {
  if (key < 0 || NKEY <= key) {
    cprintf("key error\n");
    return -1;
  }
  if (proc->shm_keys[key] != 1) {
    return -1;
  }
  acquire(&shmlock);
  proc->shm_keys[key] = 0;

  cprintf("FreeSharedPage: key is %d\n", key);
  struct sharemempage *shmem = &shmarray[key];
  cprintf("FreeSharedPage: refcount is %d\n",shmem->refcount);
  shmem->refcount--;
  cprintf("FreeSharedPage: page_nums is %d\n", page_nums[key]);
  if (shmem->refcount == 0) {
    for (int i = 0; i < page_nums[key]; i++) {
      kfree((char *) P2V(shmem->physaddr[i]));        //Recycle physical memory, page by page, frame by frame
    }
  }
  //If the current key without proc are 0, then free the space
  int flag = 0;
  for (int i = 0; i < NKEY; i++) {
    if (proc->shm_keys[key] == 1) {
      flag++;
      break;
    }
  }
  if (flag == 0) {
    // Free the user space memory, because the add is unordered, the calculation of the free virtual
    // address is very complicated, only a one-time free
    deallocshmex(proc->pgdir, proc->top, KERNBASE);
    cprintf("Free the user space memory.\n");
  }
  release(&shmlock);
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

