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
//#include "lib/kernel/console.c"


struct fd_elem {
	int fd;
	struct file *File;
	struct list_elem elem;
//	struct list_elem thread_elem;
};

static struct list file_list; 

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
static int get_user_byte (const uint32_t *uaddr);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

static struct file *find_file_by_fd (int fd);
static struct fd_elem *find_fd_elem_by_fd (int fd);
static int alloc_fid (void);
static struct fd_elem *find_fd_elem_by_fd_in_process (int fd);


void syscall_init (void) {
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init(&file_list);
}


static void syscall_handler (struct intr_frame *f /*UNUSED*/) {
	
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
	t = thread_current ();   
	t->status = status;
	thread_exit ();
	return;
}

static int read (int fd, void *buffer, unsigned size){	
	struct file *File;
	int ret = -1;
	unsigned i;

	if(fd == 0){
		for (i = 0; i < size; i++)
			*(uint8_t *)(buffer + i) = input_getc ();
		ret = size;
	}
	else if (fd == 1){
		exit(-1);
	}
	else if(!is_user_vaddr (buffer) || !is_user_vaddr (buffer + size)){
		exit(-1);
	}
	else{
		File = find_file_by_fd(fd);
		if(!File)
			return -1;
		
		ret = file_read(File, buffer, size);
	}
	return ret;	
}

static int open (const char *File){
	struct file *F;
	struct fd_elem *FDE;
	int ret = -1;
	
	if (!File)
		return ret;
	if (!is_user_vaddr (File))
		exit (-1);
	
	F = filesys_open (File);
	if (!F)
		return ret;

	FDE = (struct fd_elem *)malloc(sizeof(struct fd_elem));
	if (!FDE){
	  file_close (F);
	  return ret;
	}

	FDE->File = F;
	FDE->fd = alloc_fid ();
	list_push_back (&file_list, &FDE->elem);
//	list_push_back (&thread_current ()->files, &FDE->thread_elem);
	ret = FDE->fd;
	return ret;
}

static void close (int fd){
	struct fd_elem *f;

//	f = find_fd_elem_by_fd_in_process (fd);
	f = find_fd_elem_by_fd (fd);

	if (!f)
		return;
	
	file_close (f->File);
	list_remove (&f->elem);
//	list_remove (&f->thread_elem);
	free (f);
}

static pid_t exec (const char *cmd_line){
	if (!cmd_line)
		return -1;
	
	if(!is_user_vaddr (cmd_line))
		exit(-1);
		
	return process_execute (cmd_line);
}

static int wait (pid_t pid){
	return process_wait(pid);
}

static int filesize (int fd){
	struct file *File;
	File = find_file_by_fd (fd);
	if (!File)
		return -1;
	return file_length(File);
}

static unsigned tell (int fd){
	struct file *File;
	File = find_file_by_fd (fd);
	if (!File)
		return -1;
	return file_tell (File);
}

static void seek (int fd, unsigned position){
	struct file *File;
	File = find_file_by_fd (fd);
	if (!File)
		return;
	file_seek (File, position);
	return;
}

static void halt (void){
	power_off();
}

static int write (int fd, const void *buffer, unsigned size){
//	printf("%s", (char*)buffer);
	struct file *File;
	int ret = -1;
//	unsigned i = 0;
	if(fd == STDOUT_FILENO){
//		char * s = (char *)buffer;
		printf("yippee 1\n");
//		for(i = 0; i < size; i++)
//			putchar ((int)*(s+i));
		putbuf(buffer, size);
	}
	else if (fd == STDIN_FILENO){
		printf("yippee 2\n");
		exit(-1);
	}
	else if(!is_user_vaddr (buffer) || !is_user_vaddr (buffer + size)){
		exit(-1);
	}
	else{
		File = find_file_by_fd(fd);
		if(!File){
			printf("yippee 3\n");
			return -1;
		}
		
		ret = file_write(File, buffer, size);
	}
//	printf("check \n");
	return ret;	
}

static bool create (const char *file, unsigned initial_size){
	if (!file)
		return false;
	else
		return filesys_create (file, initial_size);
}

static bool remove (const char *file){
	if(!file)
		return false;
	
	if(!is_user_vaddr(file))
		exit(-1);
	
	return filesys_remove(file);
}
	
static int alloc_fid(void){
	int fid = 2;
	while(find_fd_elem_by_fd(fid)!=NULL){
		fid++;
	}
	return fid;
}

static struct file * find_file_by_fd (int fd) {
  struct fd_elem *ret;
  ret = find_fd_elem_by_fd (fd);
  if (!ret)
    return NULL;
  return ret->File;
}

static struct fd_elem *find_fd_elem_by_fd (int fd) {
	struct fd_elem *ret;
	struct list_elem *l;

	for (l = list_begin (&file_list); l != list_end (&file_list); l = list_next (l)){
		ret = list_entry (l, struct fd_elem, elem);
		if (ret->fd == fd)
			return ret;
	}

	return NULL;
}

/*
static struct fd_elem *find_fd_elem_by_fd_in_process (int fd) {
	struct fd_elem *ret;
	struct list_elem *l;
	struct thread *t;

	t = thread_current ();

	for (l = list_begin (&t->files); l != list_end (&t->files); l = list_next (l)) {
		ret = list_entry (l, struct fd_elem, thread_elem);
		if (ret->fd == fd)
			return ret;
	}

	return NULL;
}
*/	

// Lab 2 Implementation
/* Reads 4 BYTES at user virtual address UADDR.
 * UADDR must be below PHYS_BASE.
 * Returns the byte value if successful, -1 if a segfault
 * occurred. */
 
static int
get_user_byte (const uint32_t *uaddr)		// Modified get_user to return 32-bit(4 bytes) result to the calling function 
{
  int result;
  // Checking validity of the passed address
  if(!is_user_vaddr(uaddr))
  {
      return -1;
  }
  
/* calculating 32-bit(4 BYTES) value by using the given get_user function 
 * and shifting the obtained values by appropriate amount to get the 32-bit address.*/
 
  else
  {
	  result=get_user((const uint8_t *)uaddr) + ( get_user((const uint8_t *)uaddr)<<8 ) + ( get_user((const uint8_t *)uaddr)<<16 ) + ( get_user((const uint8_t *)uaddr)<<24 );
	  return result;		
  }   
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
        return false;
  }
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code!=(-1);
}

// Lab 2 Implementation end
