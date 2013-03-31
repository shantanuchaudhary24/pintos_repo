#ifndef USERPROG_SYSCALL_H
//32306
#define USERPROG_SYSCALL_H

typedef int pid_t;


// Variable file lock used for locking files during read/write system call
static struct lock fileLock;

void syscall_init (void);
void terminate_process(void);

#endif /* userprog/syscall.h */
