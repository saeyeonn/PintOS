#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct child_elem
  {
    struct thread * child;
    struct list_elem e;
    tid_t tid;
    int exit_status;
  };

#endif /* userprog/process.h */