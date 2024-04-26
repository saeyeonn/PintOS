#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif


/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b


/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

// alarm clock
static struct list sleep_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

static int64_t min_tick;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

// alarm clock
static long long min_tick;

/* Load average of ready_list, used for recalculate priority. */
static fixed_point load_avg; // modify

#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  // alarm clock - 1
  list_init (&sleep_list); 
  min_tick = INT64_MAX;

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();

  load_avg = 0; // bsd schedular ...why
}

// bsd schedular - 8
void initialize_idle_thread(void)
{
  static struct semaphore idle_started;
  sema_init(&idle_started, 0);
  thread_create("idle", PRI_MIN, idle, &idle_started);
  sema_down(&idle_started);
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */

// bsd schedular - 8
void thread_start (void) 
{
  initialize_idle_thread();
  intr_enable();
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */

// bsd schedular - 8
void update_thread_statistics(struct thread *t)
{
  if (t == idle_thread) idle_ticks++;

  #ifdef USERPROG
  else if (t->pagedir != NULL) user_ticks++;
  #endif

  else kernel_ticks++;
}

// bsd schedular - 8
void check_for_preemption(void)
{
  if (++thread_ticks >= TIME_SLICE) intr_yield_on_return();
}

// bsd schedular - 8
void thread_tick (void) 
{
  struct thread *current_thread = thread_current ();

  update_thread_statistics(current_thread);
  check_for_preemption();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  // priority scheduling - 1
  if (check_highest_preemption()) thread_yield();

  return tid;
}


// alarm clock - 2
void thread_sleep(int64_t ticks)
{
    struct thread *current_thread = thread_current();
    enum intr_level old_level;

    ASSERT(!intr_context());
    
    old_level = intr_disable();

    current_thread->wakeup_tick = ticks;

    if (list_empty(&sleep_list) || ticks < min_tick) set_min_tick(ticks);
    if (current_thread != idle_thread) list_push_back(&sleep_list, &current_thread->elem);

    thread_block();
    intr_set_level(old_level);
}

bool thread_compare_wakeup_tick(const struct list_elem *elem_list_1, const struct list_elem *elem_list_2, void *aux UNUSED)
{
   const int64_t tick_1 = list_entry(elem_list_1, struct thread, elem)->wakeup_tick;
   const int64_t tick_2 = list_entry(elem_list_2, struct thread, elem)->wakeup_tick;
   
   //return tick_1 <= tick_2;
   return tick_1 < tick_2;
}

bool thread_compare_priority(const struct list_elem *elem_list_1, const struct list_elem *elem_list_2, void *aux UNUSED)
{
   const int priority_1 = list_entry(elem_list_1, struct thread, elem)->priority;
   const int priority_2 = list_entry(elem_list_2, struct thread, elem)->priority;

   if (priority_1 > priority_2) return true;

   return false;
}

void set_min_tick(int64_t ticks)
{
  if (min_tick > ticks) min_tick = ticks;
}

int64_t get_min_tick(void)
{
  return min_tick;
}

// alarm clock - 2, priority scheduling - 3
void thread_wakeup(int64_t ticks)
{
  min_tick = INT64_MAX;
  struct list_elem *elem_list = list_begin(&sleep_list);
    
  while (!(elem_list == list_end(&sleep_list)))
  {
    struct thread *thr = list_entry(elem_list, struct thread, elem);
        
      if (thr -> wakeup_tick > ticks)
      {
        set_min_tick(thr-> wakeup_tick);
        elem_list = list_next(elem_list);
      }
      else
      {
        elem_list = list_remove(&thr->elem);
        thread_unblock(thr);
      }
  }
}


void sort_ready_list(void) 
{
   list_sort(&ready_list, thread_compare_priority, NULL);
}


// priority scheduling
bool check_highest_preemption()
{
   if (list_empty(&ready_list)) return false;

   struct thread *current_thread = thread_current();
   struct thread *next_thread = list_entry(list_front(&ready_list), struct thread, elem);
   
   if (current_thread ->priority < next_thread-> priority) return true;

   return false;
}


/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  // list_push_back (&ready_list, &t->elem); original code
  
  // priority scheduling - 3
  list_insert_ordered(&ready_list, &t->elem, thread_compare_priority, NULL);
  
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
  {
    // list_push_back (&ready_list, &cur->elem); origin code

    // priority scheduling - 4
    list_insert_ordered(&ready_list, &cur->elem, thread_compare_priority, NULL);
  }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

// priority scheudling - 2 , 12
void thread_set_priority (int new_priority) 
{
  // bsd schedular - 2
  if (thread_mlfqs) return ;

  struct thread *current_thread = thread_current();
  
  if (current_thread -> original_priority == current_thread -> priority || new_priority > current_thread -> priority)
    current_thread -> priority = new_priority;      
  
  current_thread -> original_priority = new_priority;
  
  if (check_highest_preemption()) thread_yield();
  // thread_current ()->priority = new_priority; original code
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) // priority
{
  enum intr_level old_level = intr_disable(); 
  int return_value = thread_current()->priority; 
   
  intr_set_level(old_level); 

  return return_value; 
}

/* Sets the current thread's nice value to NICE. */

/* Returns the current thread's nice value. */

// bsd schedular - 9
int thread_get_nice (void) 
{
   return thread_current() -> nice; 
}

fixed_point fixed_point_add_int(fixed_point x, int n)
{
	return x + n * bits;
}

// bsd schedular - done
void increase_recent_cpu(struct thread *thr)
{
   if (thr == idle_thread) return ;
   thr -> recent_cpu = fixed_point_add_int(thr->recent_cpu, 1);
}

fixed_point fixed_point_div_by_int(fixed_point x, int n)
{
	return x / n;
}

void
update_mlfqs_priority(struct thread *t)
{
   if(t == idle_thread) return ;
   int recent_cpu = round_fixed_point(fixed_point_div_by_int(t->recent_cpu, 4));
   int new_priority = PRI_MAX - recent_cpu - 2*t->nice;
   if(new_priority < PRI_MIN) new_priority = PRI_MIN;
   if(new_priority > PRI_MAX) new_priority = PRI_MAX;
   t->priority = new_priority;
}

fixed_point fixed_point_div_fixed(fixed_point x, fixed_point y)
{
	return ((int64_t) x) * bits / y;
}

fixed_point fixed_point_mult_fixed(fixed_point x, fixed_point y)
{
	return ((int64_t) x) * y / bits;
}

void
update_recent_cpu(struct thread *t)
{
   if(t == idle_thread) return ;
   fixed_point coef = fixed_point_mult_by_int(load_avg, 2);
   coef = fixed_point_div_fixed(coef, fixed_point_add_int(coef, 1));
   t->recent_cpu = fixed_point_add_int(fixed_point_mult_fixed(coef, t->recent_cpu), t->nice);
}

// bsd scheduler
void update_all_bsd_priority()
{
  struct list_elem *elem_list = list_begin(&all_list);
  
  while(elem_list != list_end(&all_list))
  {
    struct thread *thr = list_entry(elem_list, struct thread, allelem);
    update_mlfqs_priority(thr);

    elem_list = list_next(elem_list);
  }

   sort_ready_list();
}

// bsd scheduler
void update_all_recent_cpu()
{
  struct list_elem *elem_list = list_begin(&all_list);

  while(elem_list != list_end(&all_list))
   {
      struct thread *thr = list_entry(elem_list, struct thread, allelem);
      
      // update recent cpu
      update_recent_cpu(thr);

      elem_list = list_next(elem_list);
   }
}

fixed_point fixed_point_add_fixed(fixed_point x, fixed_point y)
{
	return x + y;
}

// bsd scheduler
void update_load_avg()
{
  int ready_threads = list_size(&ready_list);
  
  if (thread_current() != idle_thread) ready_threads++;
  
  fixed_point coef = bits / 60;
  load_avg = fixed_point_add_fixed(fixed_point_mult_fixed(fixed_point_mult_by_int(coef, 59), load_avg), 
                               fixed_point_mult_by_int(coef, ready_threads));
}

// bsd schedular - 10 - done
void thread_set_nice (int nice UNUSED) 
{
  struct thread *current_thread = thread_current();
   
  if (current_thread == idle_thread) return; 
      
  current_thread -> nice = nice;
      
  // update
  int recent_cpu;
  fixed_point x = (current_thread -> recent_cpu) / 4;

  if (x >= 0) recent_cpu = (x + (bits >> 1) ) / bits;
	else recent_cpu = (x - (bits >> 1)) / bits;

  int new_priority = PRI_MAX - recent_cpu - 2 * (current_thread -> nice);

  if (new_priority > PRI_MAX) new_priority = PRI_MAX;
  if (new_priority < PRI_MIN) new_priority = PRI_MIN;
      
  current_thread -> priority = new_priority;

  if (check_highest_preemption()) thread_yield(); 
}

/* Returns 100 times the system load average. */


fixed_point round_fixed_point(fixed_point x)
{
	if(x >= 0) return (x+(bits>>1))/bits;
	else return (x-(bits>>1))/bits;
}

fixed_point fixed_point_mult_by_int(fixed_point x, int n)
{
	return x * n;
}

/* Returns 100 times the current thread's recent_cpu value. */
// bsd schedular - 7 - done
int thread_get_recent_cpu ()
{
   return round_fixed_point(fixed_point_mult_by_int(thread_current()->recent_cpu, 100)); 
}

// bsd schedular - 6
int 
thread_get_load_avg (void) 
{
  return round_fixed_point(fixed_point_mult_by_int(load_avg, 100)); 
}


/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;
  // list_push_back (&all_list, &t->allelem);

  // priority scheduling - 11
  t -> waiting_lock = NULL;
  list_init(&t -> current_locks);
  t -> original_priority = priority;

  // bsd schedular - 1
  if (thread_mlfqs)
  {
    t -> recent_cpu = 0;
    t -> nice = 0;
  }

  enum intr_level old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem); 
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

// priority scheduling - 9
void update_priority(struct thread *current_thread)
{
	int new_priority = current_thread -> original_priority;

	if (list_empty(&current_thread -> current_locks)) 
	{
		current_thread -> priority = new_priority;
		return ;
	}

  struct list_elem *elem_list;

	for (elem_list = list_begin(&current_thread -> current_locks);
	        elem_list != list_end(&current_thread -> current_locks); 
          elem_list = list_next(elem_list))
	{
		struct semaphore *sema = &list_entry(elem_list, struct lock, elem) -> semaphore;
		
    if (!list_empty(&sema -> waiters))
		{
			list_sort(&sema -> waiters, thread_compare_priority, NULL);
			int compare_priority = list_entry(list_begin(&sema -> waiters), struct thread, elem) -> priority;
			
      if (new_priority < compare_priority) new_priority = compare_priority;
		}
	}
	current_thread -> priority = new_priority;

	return ;
}

// priority scheduling - 10
void donate_priority(struct thread *current_thread)
{
	struct thread* holder = current_thread -> waiting_lock -> holder;
	int current_priority = current_thread -> priority;
	bool ready_flag = false;	

	while (holder)
	{
		if (holder -> priority >= current_priority) break;
    else
		{
			if (holder -> status == THREAD_READY) ready_flag = true;
			holder -> priority = current_priority;
		}

		if (holder -> waiting_lock) holder = holder -> waiting_lock -> holder;
	}

  if (ready_flag) sort_ready_list(); //!!! Advanced Scheduling
}

