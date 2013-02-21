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
	struct list_elem thread_elem;
};

static struct list fileList; 
static struct lock fileLock;
static struct file *findFile (int fd);
static struct fd_elem *findFdElem (int fd);
static int alloc_fid (void);
static struct fd_elem *findFdElemProcess (int fd);

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
static int get_user_byte(int *uaddr);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);


void syscall_init (void) {
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init(&fileList);
  lock_init(&fileLock);
}


static void syscall_handler (struct intr_frame *f /*UNUSED*/) {
	
	/*printf ("system call!\n");
	thread_exit ();*/
	int *p;
	unsigned ret = 0;
    int arg1=0,arg2=0,arg3=0;
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
			arg1=get_user_byte((p + 1));
			if (arg1==(-1))
				exit(-1);						  
			close(arg1); 
			break;
		case SYS_CREATE : 
			arg1=get_user_byte(p+1);
			arg2=get_user_byte(p+2);
			if (arg1==(-1) || arg2==(-1))
				exit(-1);						  
			ret = (unsigned)create((const char*)arg1, (unsigned)arg2);
			f->eax = ret; 
			break;
		case SYS_EXEC : 
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)exec((const char*)arg1);
			f->eax = ret; 
			break;
		case SYS_EXIT : 
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			exit(arg1);
			break;
		case SYS_FILESIZE : 
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)filesize(arg1);
			f->eax = ret; 
			break;
		case SYS_OPEN : 
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)open((const char*)(arg1));
			f->eax = ret; 
			break;
		case SYS_WAIT :
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)wait((pid_t)arg1);
			f->eax = ret; 
			break;
		case SYS_REMOVE : 
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)remove((const char*)arg1);
			f->eax = ret; 
			break;
		case SYS_READ :
			arg1=get_user_byte(p+1);
			arg2=get_user_byte(p+2);
			arg3=get_user_byte(p+3);
			if (arg1==(-1) || arg2==(-1) || arg3==(-1))
				exit(-1);						  
			ret = (unsigned)read(arg1, (void*)arg2, (unsigned)arg3);
			f->eax = ret; 
			break;
		case SYS_WRITE : 
			arg1=get_user_byte(p+1);
			arg2=get_user_byte(p+2);
			arg3=get_user_byte(p+3);
			if (arg1==(-1) || arg2==(-1) || arg3==(-1))
				exit(-1);						  
			ret = (unsigned)write(arg1, (const void *)arg2, (unsigned)arg3);
			f->eax = ret; 
			break;
		case SYS_SEEK :  
			arg1=get_user_byte(p+1);
			arg2=get_user_byte(p+2);
			if (arg1==(-1) || arg2==(-1))
				exit(-1);						  
			seek(arg1, (unsigned)arg2);
			break; 
		case SYS_TELL :  
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)tell(arg1);
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
	struct list_elem *l;
	t = thread_current ();   
	//~ while(!list_empty(&t->files)){
		//~ l = list_begin(&t->files);
		//~ printf("check\n");
		//~ close(list_entry(l, struct fd_elem, thread_elem)->fd);
	//~ }
	t->status = status;
	thread_exit ();
	return;
}

static int read (int fd, void *buffer, unsigned size){	
	struct file *File;
	int ret = -1;
	unsigned i;

	
	if(fd == 0){
		lock_acquire(&fileLock);
		for (i = 0; i < size; i++)
			*(uint8_t *)(buffer + i) = input_getc ();
		ret = size;
		lock_release(&fileLock);
	}
	else if (fd == 1){
		exit(-1);
	}
	else if(!is_user_vaddr (buffer) || !is_user_vaddr (buffer + size)){
		exit(-1);
	}
	else{
		lock_acquire(&fileLock);
		File = findFile(fd);
		if(!File)
			return -1;
		
		ret = file_read(File, buffer, size);
		lock_release(&fileLock);
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
	list_push_back (&fileList, &FDE->elem);
	list_push_back (&thread_current ()->files, &FDE->thread_elem);
	ret = FDE->fd;
	return ret;
}

static void close (int fd){
	struct fd_elem *f;

	f = findFdElemProcess (fd);
//	f = findFdElem (fd);
	
	if (!f)
		return;
	
	file_close (f->File);
	list_remove (&f->elem);
	list_remove (&f->thread_elem);
	free (f);
}

static pid_t exec (const char *cmd_line){
	int ret = -1;
	if (!cmd_line)
		return ret;
	
	if(!is_user_vaddr (cmd_line))
		exit(-1);
	lock_acquire(&fileLock);
	ret = process_execute(cmd_line);
	lock_release(&fileLock);
	return ret;
}

static int wait (pid_t pid){
	return process_wait(pid);
}

static int filesize (int fd){
	struct file *File;
	File = findFile (fd);
	if (!File)
		return -1;
	return file_length(File);
}

static unsigned tell (int fd){
	struct file *File;
	File = findFile (fd);
	if (!File)
		return -1;
	return file_tell (File);
}

static void seek (int fd, unsigned position){
	struct file *File;
	File = findFile (fd);
	if (!File)
		return;
	file_seek (File, position);
	return;
}

static void halt (void){
	power_off();
}

static int write (int fd, const void *buffer, unsigned size){
	struct file *File;
	int ret = -1;
	if(fd == STDOUT_FILENO){
		putbuf(buffer, size);
	}
	else if (fd == STDIN_FILENO){
		exit(-1);
	}
	else if(!is_user_vaddr (buffer) || !is_user_vaddr (buffer + size)){
		exit(-1);
	}
	else{
		lock_acquire(&fileLock);
		File = findFile(fd);
		if(!File){
			return -1;
		}
		
		ret = file_write(File, buffer, size);
		lock_release(&fileLock);
	}
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
	
	printf("file removed\n");
	return filesys_remove(file);
}
	
static int alloc_fid(void){
	int fid = 2;
	while(findFdElem(fid)!=NULL){
		fid++;
	}
	printf("allocated fid = %d\n", fid);
	return fid;
}

static struct file * findFile (int fd) {
  struct fd_elem *ret;
  ret = findFdElem (fd);
  if (!ret)
    return NULL;
  return ret->File;
}

static struct fd_elem *findFdElem (int fd) {
	struct fd_elem *ret;
	struct list_elem *l;

	for (l = list_begin (&fileList); l != list_end (&fileList); l = list_next (l)){
		ret = list_entry (l, struct fd_elem, elem);
		if (ret->fd == fd)
			return ret;
	}

	return NULL;
}


static struct fd_elem *findFdElemProcess (int fd) {
	struct fd_elem *ret;
	struct list_elem *l;
	struct thread *t;
	printf("sjdbfkjbsakjdfkjabdfjbhajdfjbajdsbfkjabksdjf\n\n\n\n\n");
	t = thread_current ();

	for (l = list_begin (&t->files); l != list_end (&t->files); l = list_next (l)) {
		ret = list_entry (l, struct fd_elem, thread_elem);
		if (ret->fd == fd)
			return ret;
	}

	return NULL;
}


// Lab 2 Implementation
/* Reads 4 BYTES at user virtual address UADDR.
 * UADDR must be below PHYS_BASE.
 * Returns the byte value if successful, -1 if a segfault
 * occurred. */
 
static int
get_user_byte (int *uaddr)		// Modified get_user to return 32-bit(4 bytes) result to the calling function 
{
  int result;
  char *s= (char *)uaddr;
  // Checking validity of the passed address
  if(!is_user_vaddr(uaddr) || uaddr==0 || uaddr==NULL )
  {
      return -1;
  }
/* calculating 32-bit(4 BYTES) value by using the given get_user function 
 * and shifting the obtained values by appropriate amount to get the 32-bit address.*/
  else
  {
	  result=get_user((uint8_t *)(s))+(get_user((uint8_t *)(s+1))<<8)+(get_user((uint8_t *)(s+2))<<16)+(get_user((uint8_t *)(s+3))<<24);
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
  asm ("movl $1f, %0; movzbl %1, %0; 1:"						// movzbl has a size of 3 bytes. This point is important for fage_fault.
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
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}
// Lab 2 Implementation end
