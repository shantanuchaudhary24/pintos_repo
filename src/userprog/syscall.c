#include "userprog/syscall.h"
//14406
#include <stdio.h>
#include "lib/stdio.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "process.h"
#include "pagedir.h"
#include "devices/input.h"
#include "vm/debug.h"
#include "userprog/mmf.h"
#include "vm/page.h"

extern struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);
static int get_valid_val(int *uaddr);
static bool put_valid_val(int *uaddr,int size);
static int get_user (const int *uaddr);
static bool put_user (int *udst, int byte);
static void buffer_check_terminate(void *buffer, unsigned size);
static void buffer_check_write(void *buffer,unsigned size);
static void string_check_terminate(char* str);
static void stack_address_check(void* esp);


void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f)
{
  int *arg;
  if(f)
  {
    stack_address_check(f->esp);
    arg=f->esp;
    thread_current()->stack=f->esp;
    // get sys call number off the stack
    int sys_call_no = get_valid_val(arg);
    if(sys_call_no== -1 || sys_call_no<SYS_HALT || sys_call_no>SYS_MUNMAP)
        {
            thread_current()->exit_status=-1;
            process_terminate();
        }
    switch(sys_call_no)
    {
      case SYS_HALT:
    	  DPRINTF_SYS("SYS_HALT\n");
    	  power_off();
    	  break;
      case SYS_EXIT:
        {
        	DPRINTF_SYS("SYS_EXIT\n");
        	int status = get_valid_val(arg+1);
        	thread_current()->exit_status = status;
        	process_terminate();
        	return;
        }
        break;
      case SYS_EXEC:
        {
        	DPRINTF_SYS("SYS_EXEC\n");
        	char* file_name = (char *)get_valid_val(arg+1);
        	string_check_terminate(file_name);
        	int tid = sys_exec(file_name);
        	f->eax = tid;
        }
        return;
      case SYS_WAIT:
        {
        	DPRINTF_SYS("SYS_WAIT\n");
        	int pid = get_valid_val(arg+1);
        	int ret = process_wait(pid);
        	f->eax = ret;
        }
        return;
      case SYS_CREATE:
        {
        	DPRINTF_SYS("SYS_CREATE\n");
        	char* file_name = (char *)get_valid_val(arg+1);
        	string_check_terminate(file_name);
        	int size = get_valid_val(arg+2);
        	f->eax = sys_create(file_name, size);
        }
        return;
      case SYS_REMOVE:
        {
        	DPRINTF_SYS("SYS_REMOVE\n");
        	char* file_name = (char*)get_valid_val(arg+1);
        	string_check_terminate(file_name);
        	f->eax = sys_remove(file_name);
        }
        return;
      case SYS_OPEN:
        {
        	DPRINTF_SYS("SYS_OPEN\n");
        	char* file_name = (char*)get_valid_val(arg+1);
        	string_check_terminate(file_name);
        	f->eax = sys_open(file_name);
        }
        return; 
      case SYS_FILESIZE:
        {
        	DPRINTF_SYS("SYS_FILESIZE\n");
        	int fd = get_valid_val(arg+1);
        	f->eax = sys_filesize(fd);
        }
        return;
      case SYS_READ:
        {
        	DPRINTF_SYS("SYS_READ\n");
        	int fd = get_valid_val(arg+1);
        	char* buf = (char*)get_valid_val(arg+2);
        	int size = get_valid_val(arg+3);
        	buffer_check_write(buf, size);
        	f->eax = sys_read(fd, buf, size);
        }
        return;
      case SYS_WRITE:
        {
        	DPRINTF_SYS("SYS_WRITE\n");
        	int fd = get_valid_val(arg+1);
        	char* buf = (char*)get_valid_val(arg+2);
        	int size = get_valid_val(arg+3);
        	buffer_check_terminate(buf, size);
        	f->eax = sys_write(fd, buf, size);
        }
        return;
      case SYS_SEEK:
        {
        	DPRINTF_SYS("SYS_SEEK");
        	int fd = get_valid_val(arg+1);
        	unsigned pos = (unsigned)get_valid_val(arg+2);
        	sys_seek(fd, pos);
        }
        return;
      case SYS_TELL:
        {
        	DPRINTF_SYS("SYS_TELL");
        	int fd = get_valid_val(arg+1);
        	f->eax = sys_tell(fd);
        }
        return;
      case SYS_CLOSE:
        {
        	DPRINTF_SYS("SYS_CLOSE");
        	int fd = get_valid_val(arg+1);
        	sys_close(fd);
        }
        return;
      case SYS_MMAP:
      {
    	  DPRINTF_SYS("SYS_MMAP");
    	  int fd=get_valid_val(arg+1);
    	  void *addr=(void *)get_valid_val(arg+2);
    	  f->eax=(unsigned)mmap(fd,addr);
      }
      return;
      case SYS_MUNMAP:
      {
    	  DPRINTF_SYS("SYS_MUNMAP");
    	  mapid_t mapping=(mapid_t)get_valid_val(arg+1);
    	  munmap(mapping);
      }
      return;
    	  /*
      case SYS_CHDIR:
      case SYS_MKDIR:
      case SYS_READDIR:
      case SYS_ISDIR:
      case SYS_INUMBER:
      */
      default:
        thread_exit();
        break;
    }
  }
  else
    thread_exit();
}

// check if the stack is valid
static void stack_address_check(void* esp)
{
  if(esp==NULL || !is_user_vaddr(esp))
  {
      thread_current()->exit_status = -1;
      process_terminate();
  }
}

// prints the exit code and terminates the process
void process_terminate(void)
{
  struct thread *t = thread_current ();
  printf ("%s: exit(%d)\n", t->name, t->exit_status);  
  thread_exit();
}


int sys_exec(char* filename)
{
  tid_t tid = process_execute(filename);
  if(tid == -1) // process execute failed, return -1
    return -1;
  else  // tid is valid
  {
    intr_disable();
    thread_block(); // block myself, until child wakes me up 
                    // with the exec_status, telling me if the elf load was successful
    intr_enable();

    struct thread* child = get_thread_from_tid(tid);
   
    if(child)
    {
      // exec_status will be -1 if load failed, hence we return -1
      // in such case
      tid = child->exec_status;
      child->parent_waiting_exec = 0;
      // child had blocked itself, unblock it
      thread_unblock(child);
    }
    return tid;
  }
}

int sys_open(char* file_name)
{
  if(!*file_name)  // empty string check
    return -1;
  struct thread *t = thread_current ();
  int i = 2;
  for(; i < FDTABLESIZE; ++i) // find the first free FD
  {
    if(!t->fd_table[i])
    {
      lock_acquire(&filesys_lock);
      struct file* fi = filesys_open(file_name);
      if(fi)
        t->fd_table[i] = fi;
      lock_release(&filesys_lock);
      if(fi)
        return i;
      else
        return -1;
    }
  }
  return -1;
}

int sys_create(char* file_name, int size)
{
  int ret;
  if(!*file_name)  // empty string check
    return 0;
  lock_acquire(&filesys_lock);
  ret = filesys_create(file_name, size);
  lock_release(&filesys_lock);
  return ret;
}

int sys_remove(char* file_name)
{
  int ret;
  if(!*file_name)  // empty string check
    return 0;
  lock_acquire(&filesys_lock);
  ret = filesys_remove(file_name);
  lock_release(&filesys_lock);
  return ret;
}

void sys_close(int fd)
{
  if(fd >= 0 && fd < FDTABLESIZE) // is valid FD?
  {
    struct thread *t = thread_current ();
    struct file* fi = t->fd_table[fd];
    if(fi)
    {
      lock_acquire(&filesys_lock);
      file_close(fi);
      lock_release(&filesys_lock);
      t->fd_table[fd] = 0;
    }
  }
}

int sys_write(int fd, void *buffer, unsigned size)
{
  if(fd == STDIN_FILENO)
    return 0;
  else if(fd == STDOUT_FILENO)
  {
    putbuf(buffer, size);
    return size;
  }
  else if(fd < FDTABLESIZE && fd > 1)
  {
    struct thread* cur = thread_current ();
    struct file* fi = cur->fd_table[fd];
    if(fi)
    {
      int ret;
      lock_acquire(&filesys_lock);
      ret = file_write(fi, buffer, size);
      lock_release(&filesys_lock);
      return ret;
    }
  }
  return 0;
}

int sys_read(int fd, void* buffer, unsigned size)
{
  if(fd == STDOUT_FILENO)
    return 0;
  else if(fd == STDIN_FILENO)
  {
    unsigned i = 0;
    char* buf = (char*) buffer;
    for(; i < size; ++i)
      buf[i] = input_getc();
    return 0;
  }
  else if(fd < FDTABLESIZE && fd > 1)
  {
    struct thread *cur = thread_current ();
    struct file* fi = cur->fd_table[fd];
    if(fi)
    {
      int ret;
      lock_acquire(&filesys_lock);
      ret = file_read(fi, buffer, size);
      lock_release(&filesys_lock);
      return ret;
    }
  }
  return 0;
}

int sys_filesize(int fd)
{
  if(fd >= 0 && fd < FDTABLESIZE)
  {
    struct thread *t = thread_current ();
    struct file* fi = t->fd_table[fd];
    if(fi)
    {
      int ret;
      lock_acquire(&filesys_lock);
      ret = file_length(fi);
      lock_release(&filesys_lock);
      return ret;
    }
  }
  return -1;
}

void sys_seek(int fd, unsigned pos)
{
  if(fd >= 0 && fd < FDTABLESIZE)
  {
    struct thread *t = thread_current ();
    struct file* fi = t->fd_table[fd];
    if(fi)
    {
      lock_acquire(&filesys_lock);
      file_seek(fi, pos);
      lock_release(&filesys_lock);
    }
  }
}

unsigned sys_tell(int fd)
{
  if(fd >= 0 && fd < FDTABLESIZE)
  {
    struct thread *t = thread_current ();
    struct file* fi = t->fd_table[fd];
    if(fi)
    {
      unsigned ret;
      lock_acquire(&filesys_lock);
      ret = file_tell(fi);
      lock_release(&filesys_lock);
      return ret;
    }
  }
  return -1;
}

/* Used to map an opened file to memory.
 * it accesses the file already opened and mapped to given fd.
 * check all the pages starting from addr till the file_length, in the
 * supptable. if any of them exists, then returns -1 as it is not possible
 * to add the file at that addess. Otherwise, it calls mmfiles_insert to
 * add the mapping.
 * */
mapid_t mmap (int fd, void *addr) {
	
	DPRINTF_SYS("mmap: begin\n");
	
	struct file *fileStruct;
	struct file *reopenedFile;
	struct thread *t = thread_current();
	int length = 0;
	int i = 0;

	if(fd < 2)
		return -1;

	if(addr == NULL || addr == 0x0 || pg_ofs(addr) != 0)
		return -1;
	if(fd >= 0 && fd < FDTABLESIZE)
		fileStruct = thread_current()->fd_table[fd];
	else return -1;

	lock_acquire(&filesys_lock);
	length = file_length(fileStruct);
	lock_release(&filesys_lock);

	if(length <= 0)
		return -1;

	for(i = 0; i < length; i+=PGSIZE)
		if(get_supptable_page(&t->suppl_page_table, addr + i,t) || pagedir_get_page(t->pagedir, addr + i))
			return -1;
	
	DPRINTF_SYS("mmap: added to supptable\n");
	
	lock_acquire(&filesys_lock);
	reopenedFile = file_reopen(fileStruct);
	lock_release(&filesys_lock);

	if(reopenedFile == NULL)
		return -1;
	
	mapid_t x = mmfiles_insert(addr, reopenedFile, length);
	
	DPRINTF_SYS("mmap: inserted to hash table\n");
	
	DPRINTF_SYS("mmap: end\n");
	
	return x;
}

/* Used to unmap a mapping. Calls the mmfiles_remove function*/
void munmap (mapid_t mapping) {
	DPRINTF_SYS("munmap: begin\n");
	mmfiles_remove (mapping);
	DPRINTF_SYS("munmap: end\n");
}

/* This function has been used to check the address of the buffer
 * indices in write system call.It scans the address index of
 * the buffer at page size offsets.It advances on the basis of buffer
 * size.If the buffer size is zero then it terminates the loop. If the
 * buffer size is greater than 1 page then it advances by decrementing
 * buffer size by 1 page size.And if the size of buffer is less than 1
 * page then it just jumps to the end of the buffer.
 * */
static void buffer_check_terminate(void *buffer, unsigned size)
{
	if (get_valid_val(buffer)== -1)
	{
		DPRINTF_SYS("get_valid_val:TERMINATE PROCESS\n");
		thread_current()->exit_status=-1;
		process_terminate();
	}
	unsigned buffer_size = size;
	void *buffer_tmp ;
	buffer_tmp= buffer;
	while(buffer_tmp != NULL)
	{
		if (get_valid_val(buffer_tmp)==-1)
		{
			DPRINTF_SYS("get_valid_val:TERMINATE PROCESS\n");
			thread_current()->exit_status=-1;
			process_terminate();
		}
		if (buffer_size == 0)
			buffer_tmp = NULL;
		else if (buffer_size > PGSIZE)
		{
			buffer_tmp += PGSIZE;
			buffer_size -= PGSIZE;
		}
		else
		{
			buffer_tmp = buffer + size - 1;
			buffer_size = 0;
		}
	}
	DPRINTF_SYS("buffer_check_terminate:PASSED\n");
}

/* This function has been used to check the address of the buffer
 * indices in read system call.It scans the address index of
 * the buffer at page size offsets.It advances on the basis of buffer
 * size.If the buffer size is zero then it terminates the loop. If the
 * buffer size is greater than 1 page then it advances by decrementing
 * buffer size by 1 page size.And if the size of buffer is less than 1
 * page then it just jumps to the end of the buffer.
 * */
void buffer_check_write(void *buffer,unsigned size)
{
	int get_value=get_valid_val(buffer);
	bool value=put_valid_val(buffer,get_value);
	if(value==false)
	{
		DPRINTF_SYS("get_valid_val:TERMINATE PROCESS\n");
		thread_current()->exit_status=-1;
		process_terminate();
	}
	unsigned buffer_size = size;
	void *buffer_tmp ;
	buffer_tmp= buffer;
	while(buffer_tmp != NULL)
	{
		get_value=get_valid_val(buffer_tmp);
		value=put_valid_val(buffer_tmp,get_value);
		if(value==false)
		{
			DPRINTF_SYS("get_valid_val:TERMINATE PROCESS\n");
			thread_current()->exit_status=-1;
			process_terminate();
		}
		if (buffer_size == 0)
			buffer_tmp = NULL;
		else if (buffer_size > PGSIZE)
		{
			buffer_tmp += PGSIZE;
			buffer_size -= PGSIZE;
		}
		else
		{
			buffer_tmp = buffer + size - 1;
			buffer_size = 0;
		}
	}
	DPRINTF_SYS("buffer_check_write:PASSED\n");
}

/* Checks the validity of the string
 * terminates the process if address is invalid
 * */
void string_check_terminate(char* str)
{
	char *tmp;
	tmp=str;
	if(get_valid_val((int *)tmp)==-1)
	{
		DPRINTF_SYS("get_valid_val:TERMINATE PROCESS\n");
		thread_current()->exit_status=-1;
		process_terminate();
	}
	while((tmp-str)<PGSIZE && *tmp!='\0')
	{
		tmp++;
		if(get_valid_val((int *)tmp)==-1)
		{
			DPRINTF_SYS("get_valid_val:TERMINATE PROCESS\n");
			thread_current()->exit_status=-1;
			process_terminate();
		}
	}
	DPRINTF_SYS("string_check:PASSED\n");
}

/* Verifies the addresses passed to it by checking if they are
 * not NULL as well as not lying in the kernel space above PHYS_BASE.
 * Then it uses get_user() to get the value stored at the passed
 * address. */
static int
get_valid_val(int *uaddr)
{
  if(!is_user_vaddr(uaddr) || uaddr==NULL )
  {
	  DPRINTF_SYS("get_valid_val:TERMINATE PROCESS\n");
	  thread_current()->exit_status=-1;
	  process_terminate();
  }
  return get_user(uaddr);
}
 
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const int *uaddr)
{
  int result;
  asm ("movl $1f, %0; movl %1, %0; 1:"		
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Function for checking the address before
 * being passed to the put_user function.
 * Checks if the address is NULL or is greater
 * than PHYS_BASE
 * */
static bool put_valid_val(int *uaddr,int byte)
{
  if(!is_user_vaddr((void *)uaddr) || uaddr==NULL  )
  {
	  DPRINTF_SYS("get_valid_val:TERMINATE PROCESS\n");
	  thread_current()->exit_status=-1;
	  process_terminate();
  }
  return put_user(uaddr,byte);
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */

static bool
put_user (int *udst, int byte)
{
  int error_code;
  asm ("movl $1f, %0; mov %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}
