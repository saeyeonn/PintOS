#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/init.h"
#include "devices/shutdown.h" 
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "devices/input.h"
#include "threads/malloc.h"

static void syscall_handler (struct intr_frame *);
void get_args (struct intr_frame *f, int * args, int num_of_args);


struct thread_file
{
    struct list_elem file_elem;
    struct file *file_addr;
    int fd;
};

struct lock fs_lock;

void
syscall_init (void)
{
  lock_init(&fs_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
    if (!is_valid_ptr((const void *) f->esp)) {
        exit(-1);
    }

    int args[3];
    void *page_ptr;
    uint32_t *pd = thread_current() -> pagedir;

		switch(*(int *) f->esp)
		{
			case SYS_HALT:
				halt();
				break;

			case SYS_EXIT:
        get_args(f, &args[0], 1);
				exit(args[0]);
				break;

			case SYS_EXEC:
				get_args(f, &args[0], 1);
        page_ptr = (void *) pagedir_get_page(pd, (const void *) args[0]);
        if (page_ptr == NULL) exit(-1);
      
        args[0] = (int) page_ptr;
				f -> eax = exec((const char *) args[0]);
				break;

			case SYS_WAIT:
				get_args(f, &args[0], 1);
				f->eax = wait((pid_t) args[0]);
				break;

			case SYS_CREATE:
				get_args(f, &args[0], 2);
        validate_buffer((void *)args[0], args[1]);
        page_ptr = pagedir_get_page(pd, (const void *) args[0]);
        if (page_ptr == NULL) exit(-1);
        args[0] = (int) page_ptr;
        f -> eax = create((const char *) args[0], (unsigned) args[1]);
				break;

			case SYS_REMOVE:
        get_args(f, &args[0], 1);
        page_ptr = pagedir_get_page(pd, (const void *) args[0]);
        if (page_ptr == NULL) exit(-1);
        
        args[0] = (int)page_ptr;
        f->eax = remove((const char *) args[0]);
				break;

			case SYS_OPEN:
        get_args(f, &args[0], 1);
        page_ptr = pagedir_get_page(pd, (const void *) args[0]);
        if (page_ptr == NULL) exit(-1);
        args[0] = (int)page_ptr;
        f -> eax = open((const char *) args[0]);
				break;

			case SYS_FILESIZE:
        get_args(f, &args[0], 1);
        f->eax = filesize(args[0]);
				break;

			case SYS_READ:
        get_args(f, &args[0], 3);
        validate_buffer((void *)args[1], args[2]);
        page_ptr = pagedir_get_page(pd, (const void *) args[1]);
        if (page_ptr == NULL) exit(-1);
        
        args[1] = (int)page_ptr;
        f->eax = read(args[0], (void *) args[1], (unsigned) args[2]);
				break;

			case SYS_WRITE:
        get_args(f, &args[0], 3);
        validate_buffer((void *)args[1], args[2]);
        page_ptr = pagedir_get_page(pd, (const void *) args[1]);
        if (page_ptr == NULL) exit(-1);
        args[1] = (int) page_ptr;
        f->eax = write(args[0], (const void *) args[1], (unsigned) args[2]);
        break;

			case SYS_SEEK:
        get_args(f, &args[0], 2);
        seek(args[0], (unsigned) args[1]);
        break;

			case SYS_TELL:
        get_args(f, &args[0], 1);
        f->eax = tell(args[0]);
        break;

			case SYS_CLOSE:
        get_args(f, &args[0], 1);
        close(args[0]);
				break;

			default:
				exit(-1);
				break;
		}
}


void halt (void)
{
	shutdown_power_off();
}


void exit (int status)
{
	thread_current() -> exit_status = status;
	printf("%s: exit(%d)\n", thread_current() -> name, status);
  thread_exit ();
}

int write(int fd, const void *buffer, unsigned len)
{
  struct list_elem *e;
  struct list *fd_list = &thread_current() -> fd_list;
  int w;
  
  lock_acquire(&fs_lock);

  if (fd == 1)
  {
    putbuf(buffer, len);
    lock_release(&fs_lock);
    
    return len;
  }

  if (fd == 0 || fd == 1 || list_empty(fd_list))
  {
    lock_release(&fs_lock);
    
    return 0;
  }

  e = list_front(fd_list);
  while (e != NULL)
  {
    struct thread_file *t = list_entry(e, struct thread_file, file_elem);
    if (t -> fd == fd)
    {
      w = (int) file_write(t -> file_addr, buffer, len);
      lock_release(&fs_lock);
      return w;
    }
    e = e -> next;
  }

  lock_release(&fs_lock);
  return 0;
}


pid_t exec (const char *cmd_line)
{
	if (!cmd_line) return -1;

  lock_acquire(&fs_lock);
	
  pid_t child_tid = process_execute(cmd_line);
  
  lock_release(&fs_lock);

	return child_tid;
}

int wait (pid_t pid)
{
  return process_wait(pid);
}

bool create (const char *file_name, unsigned size)
{
  lock_acquire(&fs_lock);

  bool status = filesys_create(file_name, size);
  
  lock_release(&fs_lock);
  
  return status;
}

bool remove (const char *file)
{
  lock_acquire(&fs_lock);
  bool was_removed = filesys_remove(file);
  lock_release(&fs_lock);
  return was_removed;
}

int open (const char *file_name)
{
  lock_acquire(&fs_lock);

  struct file* f = filesys_open(file_name);

  if(f == NULL)
  {
    lock_release(&fs_lock);
    return -1;
  }

  struct thread_file *new_file = malloc(sizeof(struct thread_file));
  new_file->file_addr = f;
  int fd = thread_current () -> cur_fd;
  thread_current () -> cur_fd++;
  new_file -> fd = fd;
  list_push_front(&thread_current () -> fd_list, &new_file -> file_elem);
  lock_release(&fs_lock);
  return fd;
}

int filesize(int fd)
{
  struct list_elem *e;

  lock_acquire(&fs_lock);

  if (list_empty(&thread_current() -> fd_list))
  {
    lock_release(&fs_lock);
    return -1;
  }

  e = list_front(&thread_current() -> fd_list);

  while (e != NULL)
  {
    struct thread_file *t = list_entry(e, struct thread_file, file_elem);
    if (t -> fd == fd)
    {
      lock_release(&fs_lock);
      return (int) file_length(t -> file_addr);
    }
    e = e->next;
  }

  lock_release(&fs_lock);

  return -1;
}


int read(int fd, void *buffer, unsigned size)
{
  struct list_elem *e;

  lock_acquire(&fs_lock);

  if (fd == 0)
  {
    lock_release(&fs_lock);
    return (int) input_getc();
  }

  if (fd == 1)
  {
    lock_release(&fs_lock);
    return 0;
  }

  if (list_empty(&thread_current() -> fd_list))
  {
    lock_release(&fs_lock);
    return 0;
  }

  e = list_front(&thread_current() -> fd_list);
  while (e != NULL)
  {
    struct thread_file *t = list_entry(e, struct thread_file, file_elem);
    if (t -> fd == fd)
    {
      lock_release(&fs_lock);
      int bytes = (int) file_read(t->file_addr, buffer, size);
      return bytes;
    }
    e = e->next;
  }

  lock_release(&fs_lock);
  return -1;
}


void seek(int fd, unsigned position)
{
  struct list_elem *e;

  lock_acquire(&fs_lock);

  if (list_empty(&thread_current() -> fd_list))
  {
    lock_release(&fs_lock);
    return;
  }

  e = list_front(&thread_current() -> fd_list);
  struct thread_file *t;

  while (e != NULL)
  {
    t = list_entry(e, struct thread_file, file_elem);
    if (t -> fd == fd)
    {
      file_seek(t -> file_addr, position);
      lock_release(&fs_lock);
      return;
    }
    e = e -> next;
  }

  lock_release(&fs_lock);
}


unsigned tell(int fd)
{
  struct list_elem *e;
  lock_acquire(&fs_lock);

  if (list_empty(&thread_current() -> fd_list))
  {
    lock_release(&fs_lock);
    return -1;
  }

  e = list_front(&thread_current() -> fd_list);
  struct thread_file *t;
  unsigned location;
  while (e != NULL)
  {
    t = list_entry(e, struct thread_file, file_elem);

    if (t -> fd == fd)
    {
      location = (unsigned) file_tell(t->file_addr);
      lock_release(&fs_lock);
      return location;
    }
    e = e -> next;
  }

  lock_release(&fs_lock);

  return -1;
}

void close(int fd)
{
  struct list_elem *e, *next;

  lock_acquire(&fs_lock);

  if (list_empty(&thread_current() -> fd_list))
  {
    lock_release(&fs_lock);
    return;
  }

  e = list_front(&thread_current() -> fd_list);
  struct thread_file *t;
  while (e != NULL)
  {
    t = list_entry(e, struct thread_file, file_elem);
    next = e -> next; 
    if (t -> fd == fd)
    {
      file_close(t -> file_addr);
      list_remove(&t -> file_elem);
      lock_release(&fs_lock);

      return;
    }
    e = next;
  }

  lock_release(&fs_lock);
}


bool is_valid_ptr(const void *usr_ptr) {
  struct thread *current_thread = thread_current();
  
  if (usr_ptr == NULL) return false;
  
  if (!is_user_vaddr(usr_ptr)) return false;
  
  if (pagedir_get_page(current_thread -> pagedir, usr_ptr) == NULL) return false;
  
  return true;
}

void validate_buffer (void *buffer, unsigned size)
{
  unsigned i = 0;
  char *ptr = (char *)buffer;
  
  while (i++ < size) if (!is_valid_ptr((const void *) ptr++)) exit(-1); 
}

void get_args (struct intr_frame *f, int *args, int num_of_args)
{
  int i = 0;
  int *ptr;
  
  while (i < num_of_args)
  {
    ptr = (int *) f->esp + i + 1;
    if (!is_valid_ptr((const void *) ptr)) exit(-1);
    args[i++] = *ptr; 
  }
}

