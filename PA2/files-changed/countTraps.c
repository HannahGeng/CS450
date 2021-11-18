#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"

void process_traps_conut_test(void)
{
    struct trap_statis_s cnt;
    int ret;
	int pid;
	
	pid = getpid();
    ret = countTraps(&cnt);

    printf(1, "get pid %d ALL Traps %d\n", pid, ret);
	if (cnt.TRAP_DIVIDE)
	printf(1, "get pid %d divide error %d\n", pid, cnt.TRAP_DIVIDE);
	if (cnt.TRAP_DEBUG)
	printf(1, "get pid %d debug exception %d\n", pid, cnt.TRAP_DEBUG);
	if (cnt.TRAP_NMI)
	printf(1, "get pid %d non-maskable interrupt %d\n", pid, cnt.TRAP_NMI);
	if (cnt.TRAP_BRKPTRAP)
    printf(1, "get pid %d breakpoint %d\n", pid, cnt.TRAP_BRKPTRAP);
	if (cnt.TRAP_OFLOW)
    printf(1, "get pid %d overflow %d\n", pid, cnt.TRAP_OFLOW );
	if (cnt.TRAP_BOUND)
    printf(1, "get pid %d bounds check %d\n", pid, cnt.TRAP_BOUND);
	if (cnt.TRAP_ILLOP)
    printf(1, "get pid %d illegal opcode %d\n", pid, cnt.TRAP_ILLOP);
	if (cnt.TRAP_DEVICE)
    printf(1, "get pid %d device not available %d\n", pid, cnt.TRAP_DEVICE);
	if (cnt.TRAP_DBLFLTRAP)
    printf(1, "get pid %d double fault %d\n", pid, cnt.TRAP_DBLFLTRAP);
	if (cnt.TRAP_COPROC)
    printf(1, "get pid %d reserved (not used since 486) %d\n", pid, cnt.TRAP_COPROC);
	if (cnt.TRAP_TRAPSS)
    printf(1, "get pid %d invalid task switch segment %d\n", pid, cnt.TRAP_TRAPSS);
	if (cnt.TRAP_SEGNP)
    printf(1, "get pid %d segment not present %d\n", pid, cnt.TRAP_SEGNP);
	if (cnt.TRAP_STRAPACK)
    printf(1, "get pid %d stack exception %d\n", pid, cnt.TRAP_STRAPACK);
	if (cnt.TRAP_GPFLTRAP)
    printf(1, "get pid %d general protection fault %d\n", pid, cnt.TRAP_GPFLTRAP);
	if (cnt.TRAP_PGFLTRAP)
    printf(1, "get pid %d page fault %d\n", pid, cnt.TRAP_PGFLTRAP);
	if (cnt.TRAP_RES)
    printf(1, "get pid %d reserved %d\n", pid, cnt.TRAP_RES);
	if (cnt.TRAP_FPERR)
    printf(1, "get pid %d floating point error %d\n", pid, cnt.TRAP_FPERR);
	if (cnt.TRAP_ALIGN)
    printf(1, "get pid %d aligment check %d\n", pid, cnt.TRAP_ALIGN);
	if (cnt.TRAP_MCHK)
    printf(1, "get pid %d machine check %d\n", pid, cnt.TRAP_MCHK);
	if (cnt.TRAP_SIMDERR)
    printf(1, "get pid %d SIMD floating point error %d\n", pid, cnt.TRAP_SIMDERR);
	if (cnt.TRAP_SYSCALL)
    printf(1, "get pid %d system call %d\n", pid, cnt.TRAP_SYSCALL);
	if (cnt.TRAP_DEFAULTRAP)
    printf(1, "get pid %d catchall %d\n", pid, cnt.TRAP_DEFAULTRAP);
	if (cnt.TRAP_IRQ_TRAPIMER)
    printf(1, "get pid %d IRQ TIMER %d\n", pid, cnt.TRAP_IRQ_TRAPIMER);
	if (cnt.TRAP_IRQ_KBD)
    printf(1, "get pid %d IRQ KBD %d\n", pid, cnt.TRAP_IRQ_KBD);
	if (cnt.TRAP_IRQ_COM1)
    printf(1, "get pid %d IRQ COM1 %d\n", pid, cnt.TRAP_IRQ_COM1);
	if (cnt.TRAP_IRQ_IDE)
    printf(1, "get pid %d IRQ IDE %d\n", pid, cnt.TRAP_IRQ_IDE);
	if (cnt.TRAP_IRQ_ERROR)
    printf(1, "get pid %d IRQ ERROR %d\n", pid, cnt.TRAP_IRQ_ERROR);
	if (cnt.TRAP_IRQ_SPURIOUS)
    printf(1, "get pid %d IRQ SPURIOUS %d\n", pid, cnt.TRAP_IRQ_SPURIOUS);

    return;
}

int
main(int argc, char *argv[])
{
	printf(1, "countTraps test 1\n");
	process_traps_conut_test();
	
	sleep(1);
	
	printf(1, "countTraps test 2\n");
	process_traps_conut_test();
	
	exit();
}