#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
//추가
#include "devices/shutdown.h"
// #include "devices/input.h"
#include "threads/vaddr.h"
// #include "userprog/pagedir.h"
#include <list.h>
// #include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"


static void syscall_handler (struct intr_frame *);
bool is_valid_ptr(const void *usr_ptr);

void
syscall_init (void) 
{
  // lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f UNUSED) 
{
  switch(*(int32_t*) (f->esp)){
    case SYS_HALT:
      halt(); 
      break;

    case SYS_EXIT:
      if (!is_valid_ptr(f->esp+4)) exit(-1);
      exit(*(int*) (f->esp+4));
      break;

    case SYS_EXEC:
      if (!is_valid_ptr(f->esp+4)) exit(-1);
      f->eax=exec((char*)*(uint32_t*) (f->esp+4));
      break;

    case SYS_WAIT:
      if (!is_valid_ptr(f->esp+4)) exit(-1);
      f->eax = wait(*(uint32_t*) (f->esp+4));
      break;
    
    case SYS_CREATE:
      if (!is_valid_ptr(f->esp+4) || !is_valid_ptr(f->esp+8)) exit(-1);
      f->eax = create((char*)*(uint32_t*)(f->esp+4), *(uint32_t*) (f->esp+8));
      break;

    case SYS_REMOVE:
      if (!is_valid_ptr(f->esp+4)) exit(-1);
      f->eax = file_remove((char*)*(uint32_t*) (f->esp+4));
      break;
    
    case SYS_OPEN:
      if (!is_valid_ptr(f->esp+4)) exit(-1);
      f->eax = open((char*)*(uint32_t*) (f->esp+4));
      break;

    case SYS_FILESIZE:
      if (!is_valid_ptr(f->esp + 4)) exit(-1);
      f->eax = filesize(*(uint32_t *)(f->esp + 4));
      break;
  
    case SYS_READ:
      if (!is_valid_ptr(f->esp + 4) || !is_valid_ptr(f->esp + 8) || !is_valid_ptr(f->esp + 12)) exit(-1);
      f->eax = read((int)*(uint32_t *)(f->esp + 4), (const void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
      break;
    
    case SYS_WRITE:
      if (!is_valid_ptr(f->esp + 4) || !is_valid_ptr(f->esp + 8) || !is_valid_ptr(f->esp + 12)) exit(-1);
      f->eax = write((int)*(uint32_t *)(f->esp + 4), (const void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
      break;

    case SYS_SEEK:
      if (!is_valid_ptr(f->esp + 4) || !is_valid_ptr(f->esp + 8)) exit(-1);
      seek((int)*(uint32_t *)(f->esp + 4), (unsigned)*(uint32_t *)(f->esp + 8));
      break;

    case SYS_TELL:
      if (!is_valid_ptr(f->esp + 4)) exit(-1);
      f->eax = tell((int)*(uint32_t *)(f->esp + 4));
      break;

    case SYS_CLOSE:
      if (!is_valid_ptr(f->esp + 4)) exit(-1);
      close(*(uint32_t *)(f->esp + 4));
      break;

    default:
      exit(-1);
      break;
  }
}

// system call 3
bool is_valid_ptr(const void *usr_ptr) {
  struct thread *cur = thread_current();

  // 1. NULL 포인터 검사
  if (usr_ptr == NULL) {
    return false;
  }

  // 2. 사용자 가상 주소 공간 내에 있는지 검사
  if (!is_user_vaddr(usr_ptr)) {
    return false;
  }

  // 3. 페이지 디렉토리에서 주소 검사
  if (pagedir_get_page(cur->pagedir, usr_ptr) == NULL) {
    return false;
  }

  // 모든 검사를 통과하면, 주어진 포인터는 유효함
  return true;
}

//system call 4
int wait(pid_t pid)
{
  return process_wait(pid);
}

//system call 5
pid_t exec (const char *cmd_line) {
  pid_t tid;
  struct thread *cur = thread_current();

  // 사용자 포인터 검증
  if (!is_user_vaddr(cmd_line)) {
    exit(-1); // cmd_line 포인터가 유효하지 않으면 현재 스레드 종료
  }

  // 현재 스레드의 child_load_status 초기화
  cur->child_load_status = 0;

  // 새로운 프로세스 실행
  tid = process_execute(cmd_line);

  // 동기화를 위한 잠금 획득
  lock_acquire(&cur->lock_child);

  // 자식 프로세스의 로딩 완료 대기
  while (cur->child_load_status == 0) {
    cond_wait(&cur->cond_child, &cur->lock_child);
  }

  // 자식 프로세스 로딩 실패 시
  if (cur->child_load_status == -1) {
    tid = -1; // exec 호출 실패를 나타내는 -1로 설정
  }

  // 잠금 해제
  lock_release(&cur->lock_child);

  // 새로운 프로세스의 tid 반환, 실패 시 -1 반환
  return tid;
}

//system call 7
void exit(int status) {
    struct thread *cur = thread_current();
    struct list_elem *e;
    // struct child_process *child = 
    // child_process->exit_status = status; // 현재 스레드의 exit_status를 업데이트

    // 부모 스레드의 children 리스트에서 현재 스레드를 찾아서 상태 정보를 업데이트
    if (cur->parent_id != TID_ERROR) {
        struct thread *parent = thread_get_by_id(cur->parent_id);
        if (parent != NULL) {
            lock_acquire(&parent->lock_child); // 부모 스레드의 children 리스트에 대한 동기화를 위해 잠금
            for (e = list_begin(&parent->children); e != list_end(&parent->children); e = list_next(e)) {
                struct child_process *child = list_entry(e, struct child_process, elem);
                if (child->tid == cur->tid) {
                    child->exit_status = status; // 자식 프로세스의 exit_status를 업데이트
                    break;
                }
            }
            lock_release(&parent->lock_child); // 잠금 해제
        }
    }

    // 현재 스레드(자식 프로세스) 종료
    thread_exit();
}

//system call 8
void halt(){
  shutdown_power_off();
}



// // //file m 2
bool creat(const char *file, unsigned initial_size){
  return filesys_create(file, initial_size);
}

// system call - 8
int process_add_file(struct file *f)
{
  struct thread *cur = thread_current();
  int i;

  for(i = 3; i < 128; i++)
  {
    if (cur-> fd_table[i] == NULL)
    {
      cur-> fd_table[i] = f;
      cur -> fd_index = i;
      return cur -> fd_index;
    }
  }

  cur -> fd_index = 128;
  return -1;
}

// system call - 9
int open(const char *file)
{
  if (file == NULL) return -1;

  lock_acquire(&filesys_lock);
  struct file *open_file = filesys_open(file);

  if(open_file == NULL) return -1;

  int fd =process_add_file(open_file);

  if (fd == -1) file_close(open_file);

  lock_release(&filesys_lock);
  return fd;
}




//system call -13
int read (int fd, void *buffer, unsigned length){
    unsigned char *buf = buffer;
    int read_length;
    if (fd <= 0 || fd >= 128) {
        return -1;
    }

    // is_valid_ptr 함수로 변경하여 버퍼 주소 유효성 검사
    if (!is_valid_ptr(buffer)) {
        // 유효하지 않은 포인터일 경우, 에러 반환
        return -1;
    }

    lock_acquire(&filesys_lock);

    if (fd == 0) {
        for (read_length = 0; read_length < length; read_length++) {
            char c = input_getc();
            if (!c) {
                break;
            }
            // 버퍼에 입력값 복사
            buf[read_length] = c;
        }
        lock_release(&filesys_lock);
        // 실제 읽은 길이 반환
        return read_length;
    } else {
        struct file *f = process_get_file(fd);
        if (f == NULL) {
            lock_release(&filesys_lock);
            return -1;
        }

        read_length = file_read(f, buffer, length);
        lock_release(&filesys_lock);
        return read_length;
    }
}


//system call -14
struct file *process_get_file(int fd_index){
  if (fd_index < 3 || fd_index >= 128){
    return NULL;
  }
  return thread_current() -> fd_table[fd_index];;
}

//system call -15
int filesize (int fd){
  struct file *f = process_get_file(fd);
  if (f == NULL){
    return -1;
  }
  return file_length(f);
}

//system call -17
bool file_remove (const char *file){
  return filesys_remove(file);
}

//system call -18
int write (int fd, const void *buffer, unsigned length){
  unsigned char *buf = buffer; 
  int write_length;
  
  if (fd <= 0 || fd >= 128) {
    return -1;
  }
  
  // 버퍼 주소의 유효성 검사
  if (!is_valid_ptr(buffer)) {
    // 주소가 유효하지 않으면 에러 처리
    return -1;
  }
  
  lock_acquire(&filesys_lock);
  
  if (fd == 1) {
    putbuf(buf, length);
    lock_release(&filesys_lock);
    return length;
  } else {
    struct file *f = process_get_file(fd);
    if (f == NULL) {
      lock_release(&filesys_lock);
      return -1;
    }
    write_length = file_write(f, buffer, length);
    lock_release(&filesys_lock);
    return write_length;
  }
}


//system call -19
void seek (int fd, unsigned position) {
    struct file *f = process_get_file(fd);
    if (f != NULL) {
      file_seek(f, position);
    } 
    else {
      exit(-1);
    }
}

//system call -20
unsigned tell(int fd) {
    struct file *f = process_get_file(fd);
    if (f == NULL) {
        exit(-1);
    }
    return file_tell(f);
}

//system call -21
void close(int fd) {
    struct file *f = process_get_file(fd);
    if (f == NULL) {
        return;
    }
    process_close_file(fd);

    if (fd <= 1 || fd <= 2) { 
        return;
    }
    file_close(f);
}

void process_close_file(int fd_index){
    struct thread *cur = thread_current();
    if (fd_index < 3 || fd_index >= 128){
      return NULL;
    }
    if (cur->fd_table[fd_index] != NULL){
      file_close(cur->fd_table[fd_index]);
      cur->fd_table[fd_index] = NULL;
    }
}

