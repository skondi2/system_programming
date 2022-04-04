# Angrave's 2019 Acme CS 241 Exam Prep		
## A.K.A. Preparing for the Final Exam & Beyond CS 241... 

Some of the questions require research (wikibook; websearch; wikipedia). 
It is accepted to work together and discuss answers, but please make an honest attempt first! 
Be ready to discuss and clear confusions & misconceptions in your last discussion section.
The final will also include pthreads, fork-exec-wait questions and virtual memory address translation. 
Be awesome. Angrave.

## 1. C 


1.	What are the differences between a library call and a system call? Include an example of each.
library call: malloc()
system call: sbrk()

System calls interact with the kernel and library calls are functions within program libraries.

2.	What is the `*` operator in C? What is the `&` operator? Give an example of each.
`*` is the dereference operator and it returns the value of the pointer. `&` is the address-of operator
and it returns the address of a variable in memory.

int* a = 0;
print(*a); # prints 0

int a = 6;
print(&a); # prints address of a

3.	When is `strlen(s)` != `1+strlen(s+1)` ?
If s is a string of null characters then both of these expressions should evaluate to 0
but the second one will evaluate to 1 b/c it assumes that the first character of s is not null
but it could be so strlen won't count it.

4.	How are C strings represented in memory? What is the wrong with `malloc(strlen(s))` when copying strings?
C strings are represented as character arrays. malloc(strlen(s)) won't allocate for the null byte
because strlen doesnt count the null byte.

5.	Implement a truncation function `void trunc(char*s,size_t max)` to ensure strings are not too long with the following edge cases.
```
if (length < max)
    strcmp(trunc(s, max), s) == 0
else if (s is NULL)
    trunc(s, max) == NULL
else
    strlen(trunc(s, max)) <= max
    // i.e. char s[]="abcdefgh; trunc(s,3); s == "abc". 
```
void trunc(char* s, size_t max) {
    s[max] = '\0';
}

6.	Complete the following function to create a deep-copy on the heap of the argv array. Set the result pointer to point to your array. The only library calls you may use are malloc and memcpy. You may not use strdup.

    `void duplicate(char **argv, char ***result);`
    void duplicate(char** argv, char** result) {
        result = malloc(sizeof(argv)/sizeof(argv[0]) + 1);

        memcpy(result, argv, sizeof(argv)/sizeof(argv[0]));
    } 

7.	Write a program that reads a series of lines from `stdin` and prints them to `stdout` using `fgets` or `getline`. Your program should stop if a read error or end of file occurs. The last text line may not have a newline char.

void read_stdin() {
    size_t buffersize = 10; // number of stdin characters to read
    while (fgets(buffer, buffersize, stdin)) {
        printf("%s\n", buffer);
    }
}

## 2. Memory 

1.	Explain how a virtual address is converted into a physical address using a multi-level page table. You may use a concrete example e.g. a 64bit machine with 4KB pages. 
NOTE: page is block of virtual memory, page frame is block of physical RAM. page table maps pages to frames.
Since 2^12 == 4KB, we see we have 10^15 pages = 2^64 / 2^12.

The virtual address is broken into different index depending on the number of page tables. By taking the indices and getting the start of the subpage tables and then using the respective indices as offsets in these subpage tables, we
can reach the physical frame and then offset that to get the physical address.

2.	Explain Knuth's and the Buddy allocation scheme. Discuss internal & external Fragmentation.
Buddy allocator splits the memory into blocks of different sizes based on their memory address. Because of this,
it's convenient when coalescing because the size of the blocks can be determined from their addresses alone.
One issue with this is it causes iternal fragmentation when the memory asked is too large for one of the small blocks
but too small for one of the larger blocks. This causes there to be wasted space (wasted memory within the allocated
blocks). 

When coalescing a free block with its previous free block, it can look at the boundary tag (idea that Knuth created) of that block to know how many bytes to jump back to coalesce the two blocks together.

External fragmentation: even though there is enough heap space, it might be divided up in a way that a continuous
block of that size is not available. (insufficient holes within the memory)

3.	What is the difference between the MMU and TLB? What is the purpose of each?
MMU is the part of the CPU that converts virtual addresses to physical address.
After MMU translating an address and gets it from RAM, its cached in the TLB. TLB stores recently accessed
physical addresses and thus helps MMU get physical address without having to go to RAM.

4.	Assuming 4KB page tables what is the page number and offset for virtual address 0x12345678  ?
Note this must be a 32 bit machine since the virtual address is 32 bits. 
2^32 addresses / 2^12 addresses per page = 2^20 pages. This means 20 bits are needed to specify the 
entry in the page table. 
page number = 12345
offset = 678

5.	What is a page fault? When is it an error? When is it not an error?
Page fault is when a process tries to access a missing address in the frame. 

It's not an error when the mapping doesn't exist but the address is valid. In that case, the OS will 
make the page and load it in memory.

It's an error when the page being asked for is only on disk. In this case, the OS will switch that page on 
disk with an existing page in memory, and this will thrash the MMU b/c it overuses the CPU if its occurs a lot. It's also an error if the process is writing to a non-writable segment of memory or reading from non-readable segment. In this case, OS will generate a SIGSEGV.

6.	What is Spatial and Temporal Locality? Swapping? Swap file? Demand Paging?
temporal locality: the resuse of data within period of time

spatial locality: use of data that are stored close to each other

swapping: switching segments of data between disk and main memory. Allows for computer to run programs and deal with
files that are bigger than main memory

swap file: computer can free up main memory by loading contents of RAM into a secondary file like a swap file. They
are a type of virtual memory because extends amount of accessible memory for a computer

demand paging: type of virtual memory management where page from disk is only loaded into main memory when requested

## 3. Processes and Threads 

1.	What resources are shared between threads in the same process?
NOTE: threads share the same memory, processes do not. 
A program is split into segments: Stack, Text, Heap, Data. The threads share all the segments except
the stack. However, the memory in another thread's stack is still available to other threads

2.	Explain the operating system actions required to perform a process context switch
context switch: save the status of a process and save into CPU and load it back so executes at the left-off
point later.

Steps:
- load state of process into CPU
- re-insert process into the applicable queue (ready queue)
- select a new process from the CPU and update any data structures

3.	Explain the actions required to perform a thread context switch to a thread in the same process
Thread switching is similar except that virtual memory is not switched, meaning TLB will not get flushed out
so all the cached memory addresses remain intact.

4.	How can a process be orphaned? What does the process do about it?
Process is orphaned when its parent finishes/terminates before it finishes. In this case, the init system process
will become its parent.

5.	How do you create a process zombie?
When the parent finishes but does not wait for child to finish.

6.	Under what conditions will a multi-threaded process exit? (List at least 4)
A process will exit when it receives any of these signals:
- SIGINT: ends the process nicely
- SIGQUIT: ends the process harshly and dumps the core
- SIGTERM: ends the process even more harshly
- SIGKILL: ends the process and doesn't allow the process to clean up anything

## 4. Scheduling 
1.	Define arrival time, pre-emption, turnaround time, waiting time and response time in the context of scheduling algorithms. What is starvation?  Which scheduling policies have the possibility of resulting in starvation?

arrival time: the time the process enters the scheduler

turnaround time: time between when the process is done executing and when the process arrived inside scheduler

pre-emption: process will be interrupted while being executed if another more-preferred process is ready
on the queue

waiting time: total time process is on the ready queue

response time: time between when the process arrives and is actually executed by CPU

starvation: one process eats up all the CPUs resources so then other processes cannot be executed
- pre-emptive schedulers prevent starvation (round robin, preemptive shortest job first)

2.	Which scheduling algorithm results the smallest average wait time?
first come first serve (executes based on order of arrival)

3.	What scheduling algorithm has the longest average response time? shortest total wait time?
shortest wait time: first come first serve
longest response time: shortest job first and priority (doesn't execute based on arrival time)

4.	Describe Round-Robin scheduling and its performance advantages and disadvantages.
For a specific time quanta, the processes are executed in order of their arrival in the queue, and then 
will be put back onto the queue after that time quanta. 

Advantages: ensures fairness when executing all the processes
Disadvantages: too many context switches will be expensive

5.	Describe the First Come First Serve (FCFS) scheduling algorithm. Explain how it leads to the convoy effect. 
If the first processes are very long and energy consuming, they might suck the resources out the CPU
without giving another other (possible shorter) processes from being executed.

6.	Describe the Pre-emptive and Non-preemptive SJF scheduling algorithms. 
executes the shortest jobs before the longer ones without interuptions in between. If preemptive, then
if a shorter job is ready on the queue then the current job will be interuppted and put back onto the queue
and that shorter job will be executed.

7.	How does the length of the time quantum affect Round-Robin scheduling? What is the problem if the quantum is too small? In the limit of large time slices Round Robin is identical to _____?
If the time quanta is too small, then there will be too many context switches which are expensive on the CPU. If 
the time quanta is too large, it'll be similar to first come first serve.

8.	What reasons might cause a scheduler switch a process from the running to the ready state?
if the scheduler algorithm is preemptive, then the process will be interuppted while running and it'll go back
into the ready queue.

## 5. Synchronization and Deadlock

1.	Define circular wait, mutual exclusion, hold and wait, and no-preemption. How are these related to deadlock?
circular wait: there is a circle in the Resource Allocation Graph
mutual exclusion: no two processes can use a resource at the same time
hold and wait: once a process obtains a resource, it keeps the resource locked
no pre-emption: nothing can force a process to give up a resource.

These are the 4 Coffman conditions that are necessary and suffcient for deadlock to occur. 

2.	What problem does the Banker's Algorithm solve?
it's a resource allocation method that avoids deadlock.

3.	What is the difference between Deadlock Prevention, Deadlock Detection and Deadlock Avoidance?
prevention: a situation where deadlock occurs might arrive but we are preventing it from actually deadlocking
avoidance: a situation where deadlock occurs will never happen
detection: deadlock is allowed to occur, and it will identify it and try to fix it

4.	Sketch how to use condition-variable based barrier to ensure your main game loop does not start until the audio and graphic threads have initialized the hardware and are ready.

pthread_cond ready;
cond_init(&ready);

while (!hardware_ready) cond_wait(&ready)

cond_broadcast(&ready);
start_graphic_audio();

5.	Implement a producer-consumer fixed sized array using condition variables and mutex lock.

6.	Create an incorrect solution to the CSP for 2 processes that breaks: i) Mutual exclusion. ii) Bounded wait.

7.	Create a reader-writer implementation that suffers from a subtle problem. Explain your subtle bug.

## 6. IPC and signals

1.	Write brief code to redirect future standard output to a file.
FILE* file = fopen("filename);
int fd = fileno(file);
dup2(fd, stdout); 

2.	Write a brief code example that uses dup2 and fork to redirect a child process output to a pipe
int pipe[2];
pipe(pipe);

int child = fork();
if (child == 0) { // in parent
    // read from pipe[0]
} else { // in child
    dup2(stdout, pipe[1]); 
}

3.	Give an example of kernel generated signal. List 2 calls that can a process can use to generate a SIGUSR1.
kernel generated signal: SIGSEGV
Generate SIGUSR1: signal() and sigaction()

4.	What signals can be caught or ignored?
SIGINT, SIGQUIT

5.	What signals cannot be caught? What is signal disposition?
SIGTERM, SIGKILL, SIGSTOP.
Signal disposition: pre-process attribute on how a signal is dealt with once received

6.	Write code that uses sigaction and a signal set to create a SIGALRM handler.
struct sigaction sa;
sa.sa_handler = myhandler;
sigemptyset(&sa.sa_mask);
sa.sa_flags = 0;
sigaction(SIGALRM, &sa, NULL);

7.	Why is it unsafe to call printf, and malloc inside a signal handler?
These functions are not reentrant safe, meaning that the function should be able to resume its
progress after interruption

## 7. Networking 

1.	Explain the purpose of `socket`, `bind`, `listen`, and `accept` functions

2.	Write brief (single-threaded) code using `getaddrinfo` to create a UDP IPv4 server. Your server should print the contents of the packet or stream to standard out until an exclamation point "!" is read.

3.	Write brief (single-threaded) code using `getaddrinfo` to create a TCP IPv4 server. Your server should print the contents of the packet or stream to standard out until an exclamation point "!" is read.

4.	Explain the main differences between using `select` and `epoll`. What are edge- and level-triggered epoll modes?

5.	Describe the services provided by TCP but not UDP. 

6.	How does TCP connection establishment work? And how does it affect latency in HTTP1.0 vs HTTP1.1?

7.	Wrap a version of read in a loop to read up to 16KB into a buffer from a pipe or socket. Handle restarts (`EINTR`), and socket-closed events (return 0).

8.	How is Domain Name System (DNS) related to IP and UDP? When does host resolution not cause traffic?

9.	What is NAT and where and why is it used? 

## 8. Files 

1.	Write code that uses `fseek`, `ftell`, `read` and `write` to copy the second half of the contents of a file to a `pipe`.

2.	Write code that uses `open`, `fstat`, `mmap` to print in reverse the contents of a file to `stderr`.

3.	Write brief code to create a symbolic link and hard link to the file /etc/password

4.	"Creating a symlink in my home directory to the file /secret.txt succeeds but creating a hard link fails" Why? 

5.	Briefly explain permission bits (including sticky and setuid bits) for files and directories.

6.	Write brief code to create a function that returns true (1) only if a path is a directory.

7.	Write brief code to recursive search user's home directory and sub-directories (use `getenv`) for a file named "xkcd-functional.png' If the file is found, print the full path to stdout.

8.	The file 'installmeplz' can't be run (it's owned by root and is not executable). Explain how to use sudo, chown and chmod shell commands, to change the ownership to you and ensure that it is executable.

## 9. File system 
Assume 10 direct blocks, a pointer to an indirect block, double-indirect, and triple indirect block, and block size 4KB.

1.	A file uses 10 direct blocks, a completely full indirect block and one double-indirect block. The latter has just one entry to a half-full indirect block. How many disk blocks does the file use, including its content, and all indirect, double-indirect blocks, but not the inode itself? A sketch would be useful.

2.	How many i-node reads are required to fetch the file access time at /var/log/dmesg ? Assume the inode of (/) is cached in memory. Would your answer change if the file was created as a symbolic link? Hard link?

3.	What information is stored in an i-node?  What file system information is not? 

4.	Using a version of stat, write code to determine a file's size and return -1 if the file does not exist, return -2 if the file is a directory or -3 if it is a symbolic link.

5.	If an i-node based file uses 10 direct and n single-indirect blocks (1 <= n <= 1024), what is the smallest and largest that the file contents can be in bytes? You can leave your answer as an expression.

6.	When would `fstat(open(path,O_RDONLY),&s)` return different information in s than `lstat(path,&s)`?

## 10. "I know the answer to one exam question because I helped write it"

Create a hard but fair 'spot the lie/mistake' multiple choice or short-answer question. Ideally, 50% can get it correct.
