#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  
  int status;
  if(argint(0, &status) < 0)
    return -1;

  exit(status << 8);
  return 0;  // not reached
}

int
sys_wait(void)
{

  int *status;
  if(argptr(0, (void*)&status, sizeof(*status)) < 0)
    return -1;
  return wait(status);

}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_date(void)
{
 struct rtcdate* d;

 if(argptr(0, (void **) &d, sizeof(struct rtcdate)) < 0 )
	 return -1;
 cmostime(d);
 return 0;
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int oldAddr;                 //Old size of the process
  int n;

  if(argint(0, &n) < 0)
    return -1;
  

  if(myproc()->sz + n >= KERNBASE || myproc()->sz + n < PGROUNDUP(myproc()->tf->esp))
    return -1;

  oldAddr = myproc()->sz;
  myproc()->sz += n;        //New size of the process

  if(n < 0 ){
    if(deallocuvm(myproc()->pgdir, oldAddr, myproc()->sz) == 0){
      cprintf("sbrk failed\n");
      return -1;
    }
  }
  lcr3(V2P(myproc()->pgdir));  // Invalidate TLB.
  return oldAddr;
}

int
sys_getprio(void){
  int pid;
  if(argint(0, &pid) < 0)
    return -1;
  return getprio(pid);
}

int sys_setprio(void)
{
  int pid;
  int prio; 
  /*
    Note 
    Enumerated types are integer types, and as such can be used anywhere other integer types can, including in implicit conversions and arithmetic operators. 
    Reference: https://en.cppreference.com/w/c/language/enum (C Reference -- Date: 2022-12-1)
  */
  if (argint(0, &pid) < 0)
    return -1;

  if (argint(1, &prio) < 0){
    return -1;
  }
  if(prio != HI_PRIO && prio != NORM_PRIO)
    return -1;
  return setprio(pid, (enum proc_prio) prio);
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
