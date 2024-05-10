#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
struct file *process_get_file(int);


/* Lock. */
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore *semaphore; /* Binary semaphore controlling access. */

    /* ++1.2 Priority Donation*/
    struct list_elem *elem; /* List element for priority donation. */
    int max_priority;      /* Max priority among the threads acquiring the lock. */
  };

struct lock filesys_lock;
  

void lock_init (struct lock *);
void lock_acquire (struct lock *);


#endif /* userprog/syscall.h */
