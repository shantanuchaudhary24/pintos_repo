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



void syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f /*UNUSED*/) 
{
/*  printf ("system call!\n");
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

void exit(int status){
	struct thread *t;
	t = thread_current ();  
	t->status = status;
	thread_exit ();
	return;
}

static int write (int fd, const void *buffer, unsigned length){
	printf("check\n");
	return -1;
}

static void halt (void){
	power_off();
}

static bool create (const char *file, unsigned initial_size){
	if (!file)
		return false;
	else
		return filesys_create (file, initial_size);
}

static int open (const char *file){return -1;}
static void close (int fd){return;}
static int read (int fd, void *buffer, unsigned size){return -1;}

static pid_t exec (const char *cmd_line){
	if (!cmd_line || !is_user_vaddr (cmd_line)) /* bad ptr */
		return -1;
	else	
		return process_execute (cmd_line);
}

static int wait (pid_t pid){return -1;}
static int filesize (int fd){return -1;}
static unsigned tell (int fd){return -1;}
static void seek (int fd, unsigned pos){return;}
static bool remove (const char *file){return false;}

