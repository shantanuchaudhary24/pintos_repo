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
#include "userprog/pagedir.h"
//#include "lib/kernel/console.c"


//LAB2 IMPLEMENTATION
// Structure for the elements in the file desciptor table
struct fd_elem {
	int fd;
	struct file *File;
	struct list_elem elem;
	struct list_elem thread_elem;
};

// Variable declaration for list of opened files (fd table)
static struct list fileList; 
// Variable file lock used for locking files during read/write system call
static struct lock fileLock;


// function for finding a file from the fd table given a fd
static struct file *findFile (int fd);
// function for finding a element of fd list given a fd
static struct fd_elem *findFdElem (int fd);
// function for allocating a new file descriptor for an newly opened file
static int alloc_fid (void);
// function for finding a file element in the list of opened files for current thread
static struct fd_elem *findFdElemProcess (int fd);
// function for handling the system calls
static void syscall_handler (struct intr_frame *);
// function for handling the write system call
static int write (int fd, const void *buffer, unsigned size);
// function for handling the halt system call
static void halt (void);
// function for handling the create system call
static bool create (const char *file, unsigned initial_size);
// function for handling the open system call
static int open (const char *file);
// function for handling the close system call
static void close (int fd);
// function for handling the read system call
static int read (int fd, void *buffer, unsigned size);
// function for handling the exec system call
static pid_t exec (const char *cmd_line);
// function for handling the wait system call
static int wait (pid_t pid);
// function for handling the filesize system call
static int filesize (int fd);
// function for handling the tell system call
static unsigned tell (int fd);
// function for handling the seek system call
static void seek (int fd, unsigned position);
// function for handling the remove system call
static bool remove (const char *file);
// function for handling the exit system call
static void exit (int status);


// functions for handling the user memory access
static int get_user_byte(int *uaddr);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
//==LAB2 IMPLEMENTATION


void syscall_init (void) {
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

	// LAB2 IMPLEMENTATION
	
	//initializing the file desciptor list
	list_init(&fileList);
	//initializing the file lock list
	lock_init(&fileLock);
	
	//== LAB2 IMPLEMENTATION
}

// System call handler
static void syscall_handler (struct intr_frame *f /*UNUSED*/) {
	
	/* 
	 * OLD IMPLEMENTATION
	printf("Systen Call!\n");
	thread_exit ();
	*/
	
// LAB2 IMPLEMENTATION
	int *p;
	unsigned ret = 0;
    int arg1 = 0, arg2 = 0, arg3 = 0;
	
	// getting the system call number using the stack pointer stored in the interrupt frame
	p = f->esp;
	
	// checking if the user is allowed to access this memory location
	if (get_user_byte (p)==-1)
		exit(-1);
	
	// checking if the system call number is out of range
	if (*p < SYS_HALT || *p > SYS_CLOSE)
		exit(-1);
	
	// corresponding to each system call, we call the corresponding handler with its args
	switch(*p){
		case SYS_HALT :
//			printf ("system call - HALT\n");
			halt();					  
			break;
		case SYS_CLOSE : 
//			printf ("system call - CLOSE\n");
			arg1=get_user_byte((p + 1));
			if (arg1==(-1))
				exit(-1);						  
			close(arg1); 
			break;
		case SYS_CREATE : 
//			printf ("system call - CREATE\n");
			arg1=get_user_byte(p+1);
			arg2=get_user_byte(p+2);
			if (arg1==(-1) || arg2==(-1))
				exit(-1);						  
			ret = (unsigned)create((const char*)arg1, (unsigned)arg2);
			f->eax = ret; 
				break;
		case SYS_EXEC : 
//			printf ("system call - EXEC\n");
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)exec((const char*)arg1);
			f->eax = ret; 
			break;
		case SYS_EXIT : 
//			printf ("system call - EXIT\n");
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			exit(arg1);
			break;
		case SYS_FILESIZE : 
//			printf ("system call - FILESIZE\n");
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)filesize(arg1);
			f->eax = ret; 
			break;
		case SYS_OPEN : 
//			printf ("system call - OPEN\n");
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)open((const char*)(arg1));
			f->eax = ret; 
			break;
		case SYS_WAIT :
//			printf ("system call - WAIT\n");
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)wait((pid_t)arg1);
			f->eax = ret; 
			break;
		case SYS_REMOVE : 
//			printf ("system call - REMOVE\n");
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)remove((const char*)arg1);
			f->eax = ret; 
			break;
		case SYS_READ :
//			printf ("system call - READ\n");
			arg1=get_user_byte(p+1);
			arg2=get_user_byte(p+2);
			arg3=get_user_byte(p+3);
			if (arg1==(-1) || arg2==(-1) || arg3==(-1))
				exit(-1);						  
			ret = (unsigned)read(arg1, (void*)arg2, (unsigned)arg3);
			f->eax = ret; 
			break;
		case SYS_WRITE : 
//			printf ("system call - WRITE\n");
			arg1=get_user_byte(p+1);
			arg2=get_user_byte(p+2);
			arg3=get_user_byte(p+3);
			if (arg1==(-1) || arg2==(-1) || arg3==(-1))
				exit(-1);						  
			ret = (unsigned)write(arg1, (const void *)arg2, (unsigned)arg3);
			f->eax = ret; 
			break;
		case SYS_SEEK :  
//			printf ("system call - SEEK\n");
			arg1=get_user_byte(p+1);
			arg2=get_user_byte(p+2);
			if (arg1==(-1) || arg2==(-1))
				exit(-1);						  
			seek(arg1, (unsigned)arg2);
			break; 
		case SYS_TELL :  
//			printf ("system call - TELL\n");
			arg1=get_user_byte(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)tell(arg1);
			f->eax = ret; 
			break;
		default:
			thread_exit();
	}
return;
//== LAB2 IMPLEMENTATION
}

// LAB2 IMPLEMENTATION
// Exit System Call 
void exit(int status){
	struct thread *t;
	struct list_elem *l;
	// get current thread
	t = thread_current ();   
	// close all the files opened in this thread
	while(!list_empty(&t->files)){
		l = list_begin(&t->files);
//		printf("check\n");
		close(list_entry(l, struct fd_elem, thread_elem)->fd);
	}
	// put exit status in the return status of the thread for future reference
	t->ret_status = status;
	// exit the thread
	thread_exit ();
	return;
}

// Read System Call
static int read (int fd, void *buffer, unsigned size){
	
	struct file *File;
	int ret = -1;
	unsigned i;
	
	// if the given file descriptor is '0'
	if(fd == 0){
		// acquire lock, get input from the console and then release lock
		lock_acquire(&fileLock);
		for (i = 0; i < size; i++)
			*(uint8_t *)(buffer + i) = input_getc ();
		ret = size;
		lock_release(&fileLock);
	}
	else if (fd == 1){
		// if fd is 1, then do nothing as we can't read from stdout
		// just return -1
		ret = -1;
	}
	else if(get_user_byte((int *)buffer)==-1 || get_user_byte((int *)(buffer + size))==-1){
		// if the memory location pointed to by buffer is not user accessible
		// exit
		exit(-1);
	}
	else{
		// else, acquire the lock, check get the file corresponding to the fd
		// write to file and then release lock
		lock_acquire(&fileLock);
		lock_acquire(&fileLock);
		File = findFile(fd);
		if(!File){
			// in case there is no file correspoinding to the fd given, exit(-1)
			lock_release(&fileLock);
			return -1;
		}
		
		ret = file_read(File, buffer, size);
		lock_release(&fileLock);
	}
	return ret;	
}

// Open System Call
static int open (const char *File){
	struct file *F;
	struct fd_elem *FDE;
	int ret = -1;
	
	// If the file name is null, return -1
	if (!File)
		return ret;
		
	// If the file name is not user accessible exit(-1)
	if (get_user_byte((int *)File)==-1) 
		exit (-1);
	
	// if not any of those things, open the file
	F = filesys_open (File);
	
	// if problem in opening, return -1
	if (!F)
		return ret;
	// allocate memory for storing details about the opened file in fd table
	FDE = (struct fd_elem *)malloc(sizeof(struct fd_elem));
	if (!FDE){
		// if problem in allocating memory, close the file and return -1
		free (FDE);
		file_close (F);
		return ret;
	}
	
	// specify all the details in the structure
	FDE->File = F;
	// allocate a new fd for the opened file
	FDE->fd = alloc_fid ();
	// push fd_elem->elem in the list
	list_push_back (&fileList, &FDE->elem);
	// push this file into the list of open files for the current thread
	list_push_back (&thread_current ()->files, &FDE->thread_elem);
	ret = FDE->fd;
	return ret;
}

// Close System Call
static void close (int fd){
	
	struct fd_elem *f;

	// find fd element for the given fd out of the list of files opened in this thread
	f = findFdElemProcess (fd);
	
	// if not found, return;
	if (!f)
		return;
	
	// else close the file and remove it out of the list
	file_close (f->File);
	list_remove (&f->elem);
	list_remove (&f->thread_elem);
	// at last free the memory occupied by this element
	free (f);
}

// Exec System Call
static pid_t exec (const char *cmd_line){
	
	int ret = -1;
	// if cmd name is null, return -1
	if (!cmd_line)
		return ret;
	
	// If the location at which it is stored is not user accessible exit(-1)
	if(get_user_byte((int *)cmd_line)==-1)
		exit(-1);
	// if none, acqure lock, execute the command, then release the lock
	lock_acquire(&fileLock);
	ret = process_execute(cmd_line);
	lock_release(&fileLock);
	// return the status returned by process execute
	return ret;
}

// Wait System Call
static int wait (pid_t pid){
	// if there is no thread by this pid, then exit(-1)
	if(!get_thread_by_tid(pid))
		exit(-1);
	// else return call process_wait with this pid
	return process_wait(pid);
}

// filesize System Call
static int filesize (int fd){
	struct file *File;
	// find file in the list of opened files
	File = findFile (fd);
	// if there is no file by that fd, return -1
	if (!File)
		return -1;
	// else call file_length to find the size of the given file
	return file_length(File);
}

// tell System Call
static unsigned tell (int fd){
	struct file *File;
	// Find the file with the given fd
	File = findFile (fd);
	// if there is no file by that fd, return -1
	if (!File)
		return -1;
		
	// else call file_tell to find the write offset for that file
	return file_tell (File);
}


// seek System Call
static void seek (int fd, unsigned position){
	struct file *File;
	// Find the file with the given fd
	File = findFile (fd);
	// if there is no file by that fd, return -1
	if (!File)
		return;
	// else call file_seek to find the change the write offset for that file
	file_seek (File, position);
	return;
}

// halt System Call
static void halt (void){
	// just call power_off, which never returns
	power_off();
	NOT_REACHED();
}

// write System Call
static int write (int fd, const void *buffer, unsigned size){
	struct file *File;
	int ret = -1;
	
	// if the given fd is of stdout
	if(fd == STDOUT_FILENO){
		// acquire lock, write buffer on the console, and then release the lock
		lock_acquire(&fileLock);
		putbuf(buffer, size);
		lock_release(&fileLock);
	}
	else if (fd == STDIN_FILENO){
		// if given fd is of the stdin, just return -1, because we can't write to stdin
		ret = -1;
	}
	else if(get_user_byte((int *)buffer)==-1 || get_user_byte((int *)(buffer + size)) == -1){
		// if the memory address of buffer to buffer+size is not user accessible, call exit(-1)
		exit(-1);
	}
	else{
		// in every other case, acquire the lock, find the file for the given fd, write to that file
		// and then release the lock
		lock_acquire(&fileLock);
		File = findFile(fd);
		if(!File){
			// in case the file is not present in the list, i.e. there is no file
			// with the given fd, then release the lock anf return -1;
			lock_release(&fileLock);
			return -1;
		}
		// write to the file and release the lock
		ret = file_write(File, buffer, size);
		lock_release(&fileLock);
	}
	return ret;	
}

// create System Call
static bool create (const char *file, unsigned initial_size){
	// if either file is null or the memory location at which it's stored is not user accessible, then exit(-1)
	if (!file || get_user_byte((int *)file) == -1)
		exit(-1);
	
	// else just call filesys_create with the given file name and file size
	return filesys_create (file, initial_size);
}

// Remove System Call
static bool remove (const char *file){
	// if file is null, then return false
	if(!file)
		return false;
	
	// if the memory location is not user accessible, then call exit(-1)
	if(!is_user_vaddr(file))
		exit(-1);
	
	// call filesys_remove with the given file name
	return filesys_remove(file);
}

// function for allocating new fid to an open file
static int alloc_fid(void){
	// initial value of fd is 2 which means no file can have a fd less than 2
	int fid = 2;
	
	// we search for files in the list with the given fid
	while(findFdElem(fid)!=NULL){
		fid++;
	}
	// and return the lowest unused fid
	return fid;
}

// function for finding the file in a fd list given a fd
static struct file * findFile (int fd) {
	struct fd_elem *ret;
	
	// find the fd element in the list for the given fd
	ret = findFdElem (fd);
	
	if (!ret)
		return NULL;
	
	// return the file associated with it
	return ret->File;
}


// function for finding the fd element in the fd list given a fd
static struct fd_elem *findFdElem (int fd) {
	struct fd_elem *ret;
	struct list_elem *l;
	
	// scan the list and compare the fd of the element in the list with the 
	// fd given and in case it maches, return that fd element
	for (l = list_begin (&fileList); l != list_end (&fileList); l = list_next (l)){
		ret = list_entry (l, struct fd_elem, elem);
		if (ret->fd == fd)
			return ret;
	}

	// if there is no sich fd element, return null
	return NULL;
}


// function for finding fd element for a given fd in the list of opened files for a given thread
static struct fd_elem *findFdElemProcess (int fd) {
	struct fd_elem *ret;
	struct list_elem *l;
	struct thread *t;
	
	// get current thread
	t = thread_current ();

	// scan the list and compare the fd of the element in the list with the 
	// fd given and in case it maches, return that fd element
	for (l = list_begin (&t->files); l != list_end (&t->files); l = list_next (l)) {
		ret = list_entry (l, struct fd_elem, thread_elem);
		if (ret->fd == fd)
			return ret;
	}

	// if there is no sich fd element, return null
	return NULL;
}

/* FUNCTION FOR CHECKING USER MEMORY ACCESS
 * Reads 4 BYTES at user virtual address UADDR.
 * UADDR must be below PHYS_BASE.
 * Returns the byte value if successful, -1 if a segfault
 * occurred. */
 
static int
get_user_byte (int *uaddr)		// Modified get_user to return 32-bit(4 bytes) result to the calling function 
{
	int result;
	char *s= (char *)uaddr;
	// Checking validity of the passed address
	if(!is_user_vaddr(uaddr) || uaddr==0 || uaddr==NULL || pagedir_get_page (thread_current()->pagedir, uaddr)==NULL)
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
 
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int get_user (const uint8_t *uaddr) {
	int result;
	asm ("movl $1f, %0; movzbl %1, %0; 1:"						// movzbl has a size of 3 bytes. This point is important for fage_fault.
		: "=&a" (result) : "m" (*uaddr));
	return result;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool put_user (uint8_t *udst, uint8_t byte) {
	int error_code;
	asm ("movl $1f, %0; movb %b2, %1; 1:"
		: "=&a" (error_code), "=m" (*udst) : "q" (byte));
	return error_code != -1;
}
//== LAB2 IMPLEMENTATION
