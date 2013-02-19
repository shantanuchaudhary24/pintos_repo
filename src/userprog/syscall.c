#include "userprog/syscall.h"
//14406
#include <stdio.h>
#include <syscall-nr.h>				// reference to library to get interrupt numbers
#include "threads/interrupt.h"		// reference to library for intr_frame 
#include "threads/thread.h"			
#include "lib/syscall-nr.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "userprog/process.h"
#include <list.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include "threads/synch.h"


static void syscall_handler (struct intr_frame *);
static int write (int fd, const void *buffer, unsigned size);
static void halt (void);
static bool create (const char *file, unsigned initial_size);
static int open (const char *file);
static void close (int fd);
static int read (int fd, void *buffer, unsigned size);
static pid_t exec (const char *cmd_line);
static int wait (pid_t pid);
static int filesize (int fd);
static unsigned tell (int fd);
static void seek (int fd, unsigned position);
static bool remove (const char *file);
static void exit (int status);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

void syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// lab2 implementation syscall handler	
static void syscall_handler (struct intr_frame *f /*UNUSED*/) 
{
	/*printf ("system call!\n");
	thread_exit ();*/
	
	int *p;
	unsigned ret = 0;
  
	p = f->esp;
  
	if (!is_user_vaddr (p))
		exit(-1);
  
	if (*p < SYS_HALT || *p > SYS_CLOSE)
		exit(-1);
    
	switch(*p){
		case SYS_HALT : 
			halt();					  
			break;
		case SYS_CLOSE : 
			if (!(is_user_vaddr (p + 1)))
				exit(-1);						  
			close(*(p + 1)); 
			break;
		case SYS_CREATE : 
			if (!(is_user_vaddr (p + 1) && is_user_vaddr (p + 2)))
				exit(-1);						  
			ret = (unsigned)create((char*)(p + 1), *(p + 2));
			f->eax = ret; 
			break;
		case SYS_EXEC : 
			if (!(is_user_vaddr (p + 1)))
				exit(-1);						  
			ret = (unsigned)exec((char*)(p + 1));
			f->eax = ret; 
			break;
		case SYS_EXIT : 
			if (!(is_user_vaddr (p + 1)))
				exit(-1);						  
			exit(*(p + 1));
			break;
		case SYS_FILESIZE : 
			if (!(is_user_vaddr (p + 1)))
				exit(-1);						  
			ret = (unsigned)filesize(*(p + 1));
			f->eax = ret; 
			break;
		case SYS_OPEN : 
			if (!(is_user_vaddr (p + 1)))
				exit(-1);						  
			ret = (unsigned)open((char*)(p + 1));
			f->eax = ret; 
			break;
		case SYS_WAIT :
			if (!(is_user_vaddr (p + 1)))
				exit(-1);						  
			ret = (unsigned)wait(*(p + 1));
			f->eax = ret; 
			break;
		case SYS_REMOVE : 
			if (!(is_user_vaddr (p + 1)))
				exit(-1);						  
			ret = (unsigned)remove((char*)(p + 1));
			f->eax = ret; 
			break;
		case SYS_READ :
			if (!(is_user_vaddr (p + 1) && is_user_vaddr (p + 2) && is_user_vaddr (p + 3)))
				exit(-1);						  
			ret = (unsigned)read(*(p + 1), (void*)(p + 2), *(p + 3));
			f->eax = ret; 
			break;
		case SYS_WRITE : 
			if (!(is_user_vaddr (p + 1) && is_user_vaddr (p + 2) && is_user_vaddr (p + 3)))
				exit(-1);						  
			ret = (unsigned)write(*(p + 1), (void*)(p + 2), *(p + 3));
			f->eax = ret; 
			break;
		case SYS_SEEK :  
			if (!(is_user_vaddr (p + 1) && is_user_vaddr (p + 2)))
				exit(-1);						  
			seek(*(p + 1), *(p + 2));
			break; 
		case SYS_TELL :  
			if (!(is_user_vaddr (p + 1)))
				exit(-1);						  
			ret = (unsigned)tell(*(p + 1));
			f->eax = ret; 
			break;
	}
return;
}
// lab2 implementation syscall_handler end
	
// lab2 implementation  Various System Call Functions defined below
// Exit System Call 
void exit(int status){
	struct thread *t;
	t = thread_current ();				// gets the current thread  
	t->status = status;					// sets the status of the thread in the status field
	thread_exit ();						// terminates the thread
	return;
}

// Halt system call
static void halt (void){
	power_off();
}

// Create system call
static bool create (const char *file, unsigned initial_size){
	if (!file)
		return false;
	else
		return filesys_create (file, initial_size);
}

// exec system call
static pid_t exec (const char *cmd_line){
	if (!cmd_line || !is_user_vaddr (cmd_line)) /* bad ptr */
		return -1;
	else	
		return process_execute (cmd_line);
}


static int write (int fd, const void *buffer, unsigned length){
	printf("check\n");
	return -1;
}

static int open (const char *file){return -1;}
static void close (int fd){return;}
static int read (int fd, void *buffer, unsigned size){return -1;}
static int wait (pid_t pid){return -1;}
static int filesize (int fd){return -1;}
static unsigned tell (int fd){return -1;}
static void seek (int fd, unsigned pos){return;}
static bool remove (const char *file){return false;}

// Lab 2 Implementation
/* Reads 4 bytes at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user_byte (const uint32_t *uaddr)						// Modified get_user to return 32-bit(4 bytes) result to the calling function 
{
  int result;												// local variable
  /*calculating 32-bit value by using the given get_user function 
    and shifting the obtained values by appropriate amount to get the 32-bit address.*/
  if(!is_user_vaddr(uaddr) || uaddr==NULL)
  {
        result=-1;
  }
  else
  {
	  result=get_user(uaddr) + ( get_user(uaddr)<<8 ) + ( get_user(uaddr)<<16 ) + ( get_user(uaddr)<<24 )
  }
  return result;		 
}
 
// Lab 2 Implementation
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  if(!is_user_vaddr(uaddr) || uaddr==NULL)
  {
        result=-1;
  }
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  if(!is_user_vaddr(udst) || udst==NULL)
  {
        error_code=-1;
  }
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}
// Lab 2 Implementation end
