#ifndef USERPROG_SYSCALL_H
//32306
#define USERPROG_SYSCALL_H

#include <stdint.h>
#include "threads/thread.h"

void syscall_init (void);
int sys_exec(char* filename);
int sys_open(char* file_name);
void sys_close(int fd);
int sys_write(int fd, void *buffer, unsigned size);
int sys_read(int fd, void* buffer, unsigned size);
int sys_filesize(int fd);
void sys_seek(int fd, unsigned pos);
unsigned sys_tell(int fd);
int sys_remove(char* file_name);
int sys_create(char* file_name, int size);
void process_terminate(void);

#endif /* userprog/syscall.h */
