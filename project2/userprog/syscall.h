#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include <debug.h>

typedef int pid_t;

void syscall_init (void);

void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int wait (pid_t);
bool create (const char *file_name, unsigned size);
bool remove (const char *file_name);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

bool is_valid_ptr(const void *usr_ptr);
void validate_buffer (void *buffer, unsigned size);

#endif /* userprog/syscall.h */