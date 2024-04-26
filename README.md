# PintOS

Pintos is a simple operating system framework for the 80×86 architecture. It supports kernel threads, loading and running user programs, and a file system, but it implements all of these in a very simple way. In the Pintos projects, you and your project team will strengthen its support in all three of these areas. You will also add a virtual memory implementation.

We will run Pintos projects in a system simulator, that is, a program that simulates an 80×86 CPU and its peripheral devices accurately enough that unmodified operating systems and software can run under it. In class we will use the Bochs and QEMU simulators

<br>

## Project 1: Threads

<br>

✅ Pass Test : 27/27

<br>

### [ Requirements ]


### 1. Alarm Clock

#### 📍 Main Goal

Current Pintos uses busy-waiting for alarm clock, so we try to modify PintOS to use sleep/wakeup instead of busy-waiting.

<br>

### 2. Priority Scheduling

#### 📍 Main goal

The current scheduler in Pintos is implemented as FIFO (First-In-First-Out), so we try to modify PintOS to priority scheduling.

- Sort the ready list by the thread priority.
- Sort the wait list for synchronization primitives(semaphore, condition variable).
- Implement the preemption.
- Preemption point: when the thread is put into the ready list (not every time when the timer interrupt is called).

We will consider 
1. priority scheduling, 
2. priority synchronization, 
3. priority donation

<br>

### 3. Advanced Scheduler

#### 📍 Main goal

- Implement 4.4 BSD scheduler MLFQ like scheduler
- Give priority to the processes with interactive nature.
- Priority based scheduler
- Prevent starvation by updating the priority
- Use nice, recent_cpu ,load_avg, for update the priority
- The Pintos kernel can only perform integer arithmetic
- Need to implement the fixed-point arithmetic for calculate the priority


<br><br>

## Project 2 : User Programs

<br>

### [ Requirements ]

### 1. Argument Passing

#### 📍 Main goal

- You need to develop a feature to parse the command line string, store it on the stack, and pass arguments to the program.
- Instead of simply passing the program file name as an argument in process_execute(), you should modify it to parse words each time a space is encountered.
- In this case, the first word is the program name, and the second and third words are the first and second arguments, respectively.

<br>

### 2. System Calls

#### 📍 Main goal

- Current Pintos doesn’t have implemented system call handlers, so system calls are not processed.
- Implement system call handler and system calls
- Add system calls to provide services to users in system call handler
- Process related system call: halt, exit, exec, wait

<br>

### 3. File manipulation

#### 📍 Main goal

- Add system calls to provide services to users in system call handler
- File related system call: create, remove, open, filesize, read, write, seek, tell, close

<br><br>

## Project 3 : coming soon...

### [ Requirements ]
