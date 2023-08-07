// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "types.h"
#include "stat.h"
#include "user.h"

#define N  1000


int
main(void)
{
  enum proc_prio prio = (enum  proc_prio) getprio(getpid());
  printf(1, "pid: %d prio: %d\n",getpid() ,prio);   
  fork();
  setprio(getpid(), HI_PRIO);
  prio = (enum  proc_prio) getprio(getpid());
  printf(1, "pid: %d prio: %d\n",getpid() ,prio);
  setprio(getpid(), 90);
  prio = (enum  proc_prio) getprio(getpid());
  printf(1, "pid: %d prio: %d\n",getpid() ,prio);
  
  for(int i = 1; i < 10000000; i++)
  {
    date((struct rtcdate*) i);
  }

  wait((int *) 0);
  exit(0);
}

 