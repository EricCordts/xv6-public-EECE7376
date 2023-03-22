#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

// Edited by Eric Cordts and Jonathan Hsin
#define NULL ((void*) 0)

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  // Edited by Eric Cordts and Jonathan Hsin
  struct proc* priorityQueue[3][NPROC]; // Need 3 queues, 0 is highest priority, 2 is lowest
  int queueCount[3];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void removeProcessFromPriorityQueue(int priority, int indexInQueue);
int findIndexInPriorityQueue(int priority);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  
  // Edited by Eric Cordts and Jonathan Hsin

  // Set each spot to null in the priority queue.
  int i;
  int j;
  acquire(&ptable.lock);
  for(i = 0; i < 3; i++)
  {
	for(j = 0; j < NPROC; j++)
	{
		ptable.priorityQueue[i][j] = NULL;
	}
  }
  ptable.queueCount[0] = 0;
  ptable.queueCount[1] = 0;
  ptable.queueCount[2] = 0;

  release(&ptable.lock);
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  // Edited by Eric Cordts and Jonathan Hsin, EECE7376
  // Default priority queue is 1
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  // Edited by Eric Cordts and Jonathan Hsin
  p->priority = 1;
  p->indexInQueue = findIndexInPriorityQueue(p->priority);
  ptable.priorityQueue[1][p->indexInQueue] = p;
  ptable.queueCount[1]++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// Requires the user has has acquired the ptable.
// Find the first index in the specified priority queue
// that is a valid spot and returns that value 
int findIndexInPriorityQueue(int priority)
{
  int i;
  for(i = 0; i < NPROC; i++)
  {
     if(ptable.priorityQueue[priority][i] == NULL)
     {
	return i;
     }
  }

  // error if there are no open spots.
  panic("Error! No open spots in priority queue\n");
  return -1;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Remove the exiting process from its priority queue
  // and decrement the queue count
  removeProcessFromPriorityQueue(curproc->priority, curproc->indexInQueue);

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    // Edited by Eric Cordts and Jonathan Hsin for EECE7376
    
    // First check the top priority queue and run it RR
    // until it is empty (queueCount == 0)
    while(ptable.queueCount[0] > 0)
    {
	int runnableInQueue0 = 0;
	int i;
	for(i = 0; i < NPROC; i++)
	{
	    p = ptable.priorityQueue[0][i];
	    if(p == NULL || p->state != RUNNABLE)
	    {
		continue;
	    }
	    // Switch to chosen process. It is the process' 
	    // job to release ptable.lock and then reacquire it 
	    // before jumping back to us.
	    runnableInQueue0 = 1;
	    c->proc = p;
	    switchuvm(p);
	    p->state = RUNNING;
	    
	    swtch(&c->scheduler, p->context);
	    switchkvm();

	    // Process is done running for now
	    // It should have changed its p->state before coming back.
	    c->proc = 0;
	}
	if(runnableInQueue0 == 0)
	{
	    break;
	}
    }
    // Then check the middle priority queue 
    // and run it RR until it is empty OR until 
    // the queueCount of the top priority queue is no longer 0.
    while(ptable.queueCount[0] == 0 && ptable.queueCount[1] > 0)
    {
	int runnableInQueue1 = 0;
	int j;
	for(j = 0; j < NPROC; j++)
	{
	    p = ptable.priorityQueue[1][j];
	    if(p == NULL || p->state != RUNNABLE)
	    {
		continue;
	    }
	    runnableInQueue1 = 1;
	    // Switch to chosen process. It is the process' job
	    // to release ptable.lock and then reacquire it before
	    // jumping back to us
	    c->proc = p;
	    switchuvm(p);
	    p->state = RUNNING;

	    swtch(&c->scheduler, p->context);
	    switchkvm();
	    // process is done running for now
	    // It should have changed its p->state before coming back.
	    c->proc = 0;

	    // Check if the queueCount of Queue 0 has changed. If so, 
	    // need to break out of the for loop.
	    if(ptable.queueCount[0] > 0)
	    {
		break;
	    }
	}
	if(runnableInQueue1 == 0)
	{
	    break;
	}
    }

    // Now check the last priority queue and run it RR 
    // until it is empty OR until the queueCount of 
    // either the top or middle queue is no longer 0.
    while(ptable.queueCount[0] == 0 && ptable.queueCount[1] == 0 && ptable.queueCount[2] > 0)
    {
	int runnableInQueue2 = 0;
	int k;
	for(k = 0; k < NPROC; k++)
	{
	    p = ptable.priorityQueue[2][k];
	    if(p == NULL || p->state != RUNNABLE)
	    {
		continue;
	    }
	    runnableInQueue2 = 1;
	    c->proc = p;
	    switchuvm(p);
	    p->state = RUNNING;
	    swtch(&c->scheduler, p->context);
	    switchkvm();
	    c->proc = 0;

	   // Check if the queueCount of Queue 0 or 1 have changed. If so, 
	   // break out of the for loop
	   if(ptable.queueCount[0] > 0 || ptable.queueCount[1] > 0)
	   {
		break;
	   } 
	}
	if(runnableInQueue2 == 0)
	{
	   break;
	}
    }
    /*
    COMMENTED OUT RR Scheduler
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();
      cprintf("Name of proc: %s\n", p->name);
      cprintf("Count of queue0: %d\n", ptable.queueCount[0]);
      cprintf("Count of queue1: %d\n", ptable.queueCount[1]);
      cprintf("Count of queue2: %d\n", ptable.queueCount[2]);
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }*/
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc* curproc = myproc();
  acquire(&ptable.lock);  //DOC: yieldlock
  // Edited by Jonathan Hsin and Eric Cordts

  // Process has used up its alloted time slot, 
  // so bump it down in priority (move it towards 2) 
  if(curproc->priority < 2)
  {
      // if it needs to be bumped down in priorty, 
      // remove it from its current queue
      // and add it to the new queue. 
      ptable.priorityQueue[curproc->priority][curproc->indexInQueue] = NULL;
      ptable.queueCount[curproc->priority]--;
      curproc->priority++;
      curproc->indexInQueue = findIndexInPriorityQueue(curproc->priority);
      ptable.priorityQueue[curproc->priority][curproc->indexInQueue] = curproc;
      ptable.queueCount[curproc->priority]++;
  }
  // Otherwise, it is already at the lowest 
  // priority queue, so just leave it as is.

  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;

      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// Added by Eric Cordts and Jonathan Hsin for EECE7376
// helper function to remove a killed process from the priority queue
void removeProcessFromPriorityQueue(int priority, int indexInQueue)
{
   ptable.priorityQueue[priority][indexInQueue] = NULL;
   ptable.queueCount[priority]--;
}

// Added by Eric Cordts and Jonathan Hsin for EECE7376
// Helper function to reset the priority of all 
// running processes to the highest priority 0
// for usage during the timer interrupt. 
void resetPriority()
{
   acquire(&ptable.lock);

   int i;
   int priIndex;
   for(i = 0; i < NPROC; i++)
   {
     for(priIndex = 1; priIndex < 3; priIndex++)
     {
	struct proc* process = ptable.priorityQueue[priIndex][i];
     	if(process != NULL)
     	{
	  // Add process from priority 1/2 to priority 0
          process->priority = 0;
	  process->indexInQueue = findIndexInPriorityQueue(process->priority);
	  ptable.priorityQueue[0][process->indexInQueue] = process;
	  ptable.queueCount[0]++;

	  // Remove process from priority 1/2 queue
	  ptable.priorityQueue[priIndex][i] = NULL;
	  ptable.queueCount[priIndex]--;
     	}
     }
   }

   release(&ptable.lock);
}

// Added by Eric Cordts and Jonathan Hsin
// Implementation of renice system call
int renice(int priority, int pid)
{
   acquire(&ptable.lock);

   struct proc* p;
   int pIndex;
   for(pIndex = 0; pIndex < 3; pIndex++)
   {
	int i;
	for(i = 0; i < NPROC; i++)
	{
	    p = ptable.priorityQueue[pIndex][i];
	    if(p != NULL && p->pid == pid)
	    {
		// only make changes if the 
		// priority is different. Otherwise, its a 
		// waste of time.
		if(p->priority != priority)
		{
		   // Set priority and move to appropriate queue
		   p->priority = priority;
		   p->indexInQueue = findIndexInPriorityQueue(priority);
		   ptable.priorityQueue[priority][p->indexInQueue] = p;
		   ptable.queueCount[priority]++;

		   // Remove process from previous queue
		   ptable.priorityQueue[pIndex][i] = NULL;
		   ptable.queueCount[pIndex]--;
		}
		release(&ptable.lock);
		return 0;
	    }
	}
   }
   release(&ptable.lock);
   return -1;
}

//ps command
void proc_ps(void)
{
  struct proc *p;

  acquire(&ptable.lock);

  const char* state_info[] = {"Unnused", "Embryo", "Sleeping","Runnable", "Running", "Zombie"};
  for(p=ptable.proc; p< &ptable.proc[NPROC]; p++)
  {
    if(p->state != UNUSED){
      cprintf("Name: %s, ", p->name);
      cprintf("PID: %d, ", p->pid);
      cprintf("State: %s, ", state_info[p->state]);
      cprintf("Priority: %d, ", p->priority);
      cprintf("Parent ID: %d\n", p->parent->pid);
    }
  }

  release(&ptable.lock);
}
