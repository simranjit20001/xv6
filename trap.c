#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"


// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

extern int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
     {
      exit(tf->trapno + 1);
    }
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit(tf->trapno + 1);
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  case T_PGFLT:

    //Page fault caused by a protection violation
    if((myproc()->tf->err & PTE_P) == 1){
      cprintf("Page fault: 0x%x already present. Must be a page-protection violation \n", rcr2());
      myproc()->killed = 1;
      break;
    }

    //Page fault caused by a non-present page
    //Check if the faulting address is in the process range
    if(rcr2() > myproc()->sz || rcr2() < PGROUNDUP(myproc()->tf->esp)){
      cprintf("Page fault: 0x%x out of process range. \n", rcr2());
      myproc()->killed = 1;
      break;
    }

    //Allocate a page and map it to the faulting address
    uint va = PGROUNDDOWN(rcr2());
    char *pa = kalloc();

    //Check if the allocation was successful
    if(pa == 0)
    {
      cprintf("Out of memory");
      myproc()->killed = 1;

    }else{
      //map the page to the faulting address
      memset(pa, 0, PGSIZE);
      if(mappages(myproc()->pgdir, (void*)va, PGSIZE, V2P(pa), PTE_W|PTE_U) < 0){
        cprintf("mappages failed");
        kfree(pa);
        myproc()->killed = 1;
      }
      //Descomentar en caso de querer una salida exactamente igual a los ejemplos ofrecidos - no necesario para la funcionalidad
      // cprintf("pid %d %s: trap %d err %d on cpu %d "
      //       "eip 0x%x addr 0x%x--kill proc\n",
      //       myproc()->pid, myproc()->name, tf->trapno,
      //       tf->err, cpuid(), tf->eip, rcr2());
    }
    break;
  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }    
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit(tf->trapno + 1);

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit(tf->trapno + 1);
}
