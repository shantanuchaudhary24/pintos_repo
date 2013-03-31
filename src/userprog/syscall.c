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
#include "vm/debug.h"

// Structure for the elements in the file desciptor table
struct fd_elements {
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
static struct fd_elements *findFdElem (int fd);

// function for allocating a new file descriptor for an newly opened file
static int alloc_fid (void);

// function for finding a file element in the list of opened files for current thread
static struct fd_elements *findFdElemProcess (int fd);

// function for handling the system calls
static void syscall_handler (struct intr_frame *);

// function for handling the write system call
static int write (int fd, void *buffer, unsigned size);

// function for handling the halt system call
static void halt (void);

// function for handling the create system call
static bool create (char *file, unsigned initial_size);

// function for handling the open system call
static int open (char *file);

// function for handling the close system call
static void close (int fd);

// function for handling the read system call
static int read (int fd, void *buffer, unsigned size);

// function for handling the exec system call
static pid_t exec (char *cmd_line);

// function for handling the wait system call
static int wait (pid_t pid);

// function for handling the filesize system call
static int filesize (int fd);

// function for handling the tell system call
static unsigned tell (int fd);

// function for handling the seek system call
static void seek (int fd, unsigned position);

// function for handling the remove system call
static bool remove (char *file);

// function for handling the exit system call
static void exit (int status);

// functions for handling the user memory access
static void string_check_terminate(char *str);
static void buffer_check_terminate(void *buffer, unsigned size);
static int get_valid_val(int *uaddr);
static int get_user (const int *uaddr);
void terminate_process(void);

//static mapid_t mmap (int, void *);
//static void munmap (mapid_t);


void syscall_init (void) {
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

	//initializing the file desciptor list
	list_init(&fileList);

	//initializing the file lock list
	lock_init(&fileLock);
}

// System call handler
/* The function gets the system call number using the stack pointer stored
 * in the interrupt frame. First checking if the user is allowed to access
 * this memory location (using get_valid_val()).Then we check if the system 
 * call number is out of range. If so, we exit(-1). Now, corresponding to 
 * each system call, we call the corresponding handler with its arguments.
 * */
static void syscall_handler (struct intr_frame *f /*UNUSED*/)
{
	struct thread *t = thread_current();
	int *p;
	unsigned ret = 0;
    int arg1 = 0, arg2 = 0, arg3 = 0;
	p = f->esp;
	t->stack = f->esp ;				/* Saving stack pointer (needed to handle kernel page fault) */
	if (get_valid_val (p)== -1)
		exit(-1);
	if (*p < SYS_HALT || *p > SYS_CLOSE)
		exit(-1);
	switch(*p){
		case SYS_HALT :
			DPRINTF("SYS_HALT\n");
			halt();					  
			break;
		case SYS_CLOSE : 
			DPRINTF("SYS_CLOSE\n");
			arg1=get_valid_val((p + 1));
			if (arg1==(-1))
				exit(-1);						  
			close(arg1); 
			break;
		case SYS_CREATE : 
			DPRINTF("SYS_CREATE\n");
			arg1=get_valid_val(p+1);
			arg2=get_valid_val(p+2);
			if (arg1==(-1) || arg2==(-1))
				exit(-1);						  
			ret = (unsigned)create((char*)arg1, (unsigned)arg2);
			f->eax = ret; 
				break;
		case SYS_EXEC : 
			DPRINTF("SYS_EXEC\n");
			arg1=get_valid_val(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)exec((char*)arg1);
			f->eax = ret; 
			break;
		case SYS_EXIT : 
			DPRINTF("SYS_EXIT\n");
			arg1=get_valid_val(p+1);
			if (arg1==(-1))
				exit(-1);						  
			exit(arg1);
			break;
		case SYS_FILESIZE : 
			DPRINTF("SYS_FILESIZE\n");
			arg1=get_valid_val(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)filesize(arg1);
			f->eax = ret; 
			break;
		case SYS_OPEN : 
			DPRINTF("SYS_OPEN\n");
			arg1=get_valid_val(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)open((char*)(arg1));
			f->eax = ret; 
			break;
		case SYS_WAIT :
			DPRINTF("SYS_WAIT\n");
			arg1=get_valid_val(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)wait((pid_t)arg1);
			f->eax = ret; 
			break;
		case SYS_REMOVE : 
			DPRINTF("SYS_REMOVE\n");
			arg1=get_valid_val(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)remove((char*)arg1);
			f->eax = ret; 
			break;
		case SYS_READ :
			DPRINTF("SYS_READ\n");
			arg1=get_valid_val(p+1);
			arg2=get_valid_val(p+2);
			arg3=get_valid_val(p+3);
			if (arg1==(-1) || arg2==(-1) || arg3==(-1))
				exit(-1);						  
			ret = (unsigned)read(arg1, (void*)arg2, (unsigned)arg3);
			f->eax = ret; 
			break;
		case SYS_WRITE : 
			DPRINTF("SYS_WRITE\n");
			arg1=get_valid_val(p+1);
			arg2=get_valid_val(p+2);
			arg3=get_valid_val(p+3);
			if (arg1==(-1) || arg2==(-1) || arg3==(-1))
				exit(-1);						  
			ret = (unsigned)write(arg1, (void *)arg2, (unsigned)arg3);
			f->eax = ret; 
			break;
		case SYS_SEEK :  
			DPRINTF("SYS_SEEK\n");
			arg1=get_valid_val(p+1);
			arg2=get_valid_val(p+2);
			if (arg1==(-1) || arg2==(-1))
				exit(-1);						  
			seek(arg1, (unsigned)arg2);
			break; 
		case SYS_TELL :  
			DPRINTF("SYS_TELL\n");
			arg1=get_valid_val(p+1);
			if (arg1==(-1))
				exit(-1);						  
			ret = (unsigned)tell(arg1);
			f->eax = ret; 
			break;
		/*
		case SYS_MMAP :
			arg1 = get_valid_val(p+1);
			arg2 = get_valid_val(p+2);
			if(arg1==-1 || arg2 == -1)
				exit(-1);
			ret = (unsigned)mmap(arg1, (void *)arg2);
			f->eax = ret;
			break;
		case SYS_MUNMAP:
			arg1 = get_valid_val(p+1);
			if(arg1==-1)
				exit(-1);
			munmap(arg1);
			break;
		*/
		default:
			thread_exit();
			break;
	}
return;
}


/* Gets the current thread.Closes all the files opened in this thread.
 * Puts exit status in the return status of the thread for future reference
 * Exits the thread. 
 * */
static void exit(int status){
	struct thread *t;
	struct list_elem *l;
	t = thread_current ();   
	while(!list_empty(&t->files))
	{
		l = list_begin(&t->files);
		close(list_entry(l, struct fd_elements, thread_elem)->fd);
	}
	t->ret_status = status;
	thread_exit ();
	return;
}

/* If the given file descriptor is '0',acquire lock, get input 
 * from the console and then release lock.// if fd is 1, then 
 * do nothing as we can't read from stdout just return -1.
 * If the memory location pointed to by buffer is not user accessible
 * then exit, else, acquire the lock, check get the file corresponding 
 * to the fd write to file and then release lock.
 * In case there is no file corresponding to the fd given, exit(-1)
 * */
static int read (int fd, void *buffer, unsigned size){
	
	struct file *File;
	int ret = -1;
	unsigned i;
	
	buffer_check_terminate(buffer,size);

	if(fd == 0)
	{
		lock_acquire(&fileLock);
		for (i = 0; i < size; i++)
			*(uint8_t *)(buffer + i) = input_getc ();
		ret = size;
		lock_release(&fileLock);
	}
	else if (fd == 1)
		ret = -1;
	else
	{
		lock_acquire(&fileLock);
		File = findFile(fd);
		if(!File){
			lock_release(&fileLock);
			return -1;
		}
		ret = file_read(File, buffer, size);
		lock_release(&fileLock);
	}
	return ret;	
}

/* If the file name is null,it returns -1.If the file name
 * is not user accessible exit(-1). If not any of those things,
 * opens the file. If there is a problem in opening, returns -1.
 * It then allocates memory for storing details about the opened
 * file in fd table. If there is problem in allocating memory,
 * it closes the file and returns -1. It the specifies all the
 * details in the structure.Allocates a new fd for the opened file,
 * pushes fd_elements->elem in the list,
 * pushes this file into the list of open files for the current thread
 * */
static int open (char *File){
	struct file *F;
	struct fd_elements *FDE;
	int ret = -1;
	if (!File)
		return ret;
	
	string_check_terminate(File);
	F = filesys_open (File);
	
	if (!F)
		return ret;
	FDE = (struct fd_elements *)malloc(sizeof(struct fd_elements));
	
	if (!FDE)
	{
		free (FDE);
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

/* Find fd element for the given fd out of the list of files
 * opened in this thread. If not found, return, else close 
 * the file and remove it from the list. At last free the
 * memory occupied by this element.
 * */
static void close (int fd){
	
	struct fd_elements *f;
	f = findFdElemProcess (fd);
	if (!f)
		return;
	file_close (f->File);
	list_remove (&f->elem);
	list_remove (&f->thread_elem);
	free (f);
}

/* If the location at which cmd_line is stored is not user accessible,
 * exit(-1). If cmd name(cmd_line) is null, return -1. If none, acquire
 * lock, execute the command, then release the lock and return the status
 * returned by process_execute.
 * */
static pid_t exec (char *cmd_line){
	
	int ret = -1;
	if (!cmd_line)
		return ret;

	string_check_terminate(cmd_line);

	lock_acquire(&fileLock);
	ret = process_execute(cmd_line);
	lock_release(&fileLock);
	return ret;
}

/* If there is no thread by the given pid, then exit(-1)
 * else return call process_wait with this pid
 * */
static int wait (pid_t pid){
	if(!get_thread_by_tid(pid))
		exit(-1);
	return process_wait(pid);
}

/* Finds file in the list of opened files.
 * If there is no file by that fd, returns -1
 * else it calls file_length to find the size of the given file.
 * */
static int filesize (int fd){
	struct file *File;
	File = findFile (fd);
	if (!File)
		return -1;
	return file_length(File);
}

/* Finds the file with the given fd.
 * If there is no file by that fd, it returns -1
 * else it calls file_tell to find the write offset for that file.
 * */
static unsigned tell (int fd){
	struct file *File;
	File = findFile (fd);
	if (!File)
		return -1;
	return file_tell (File);
}

/* Finds the file with the given fd.
 * If there is no file by that fd, it returns -1 else it calls
 * file_seek to find the change in the write offset for that file
 * */
static void seek (int fd, unsigned position){
	struct file *File;
	File = findFile (fd);
	if (!File)
		return;
	file_seek (File, position);
	return;
}

/* Just calls power_off, which never returns.
 * */
static void halt (void){
	power_off();
	NOT_REACHED();
}

/* If the memory address of buffer to buffer+size is not user accessible,
 * call exit(-1).  If the given fd is of the stdin, just return -1, 
 * because we can't write to stdin. If the given fd is of stdout, acquire lock,
 * write buffer on the console, and then release the lock.In every other case,
 * acquire the lock, find the file for the given fd, write to that file
 * and then release the lock. In case the file is not present in the list,
 * i.e. there is no file with the given fd, then release the lock and return -1;
 * Write to the file and release the lock and return the number of bytes written.
 * */
static int write (int fd,void *buffer, unsigned size){
	struct file *File;
	int ret = -1;
	buffer_check_terminate(buffer,size);

	if (fd == STDIN_FILENO)
		ret = -1;
	else if(fd == STDOUT_FILENO){
		lock_acquire(&fileLock);
		putbuf(buffer, size);
		lock_release(&fileLock);
	}
	else
	{
		lock_acquire(&fileLock);
		File = findFile(fd);
		if(!File)
		{ 
			lock_release(&fileLock);
			return -1;
		}
		ret = file_write(File, buffer, size);
		lock_release(&fileLock);
	}
	return ret;	
}

/* If either the file is NULL or the memory location at which
 * it is stored is not user accessible, then exit(-1)else 
 * just call filesys_create with the given file name and file size.
 * */
static bool create (char *file, unsigned initial_size){
	string_check_terminate(file);
	return filesys_create (file, initial_size);
}

/* If file is NULL, then return false
 * If the memory location is not user accessible, then call exit(-1)
 * Otherwise return filesys_remove with the given file name
 * */
static bool remove (char *file){
	string_check_terminate(file);
	if(!file)
		return false;
	return filesys_remove(file);
}

/* Function for allocating new fid to an open file.
 * Initial value of fd is 2 which means no file can have
 * a fd less than 2.Now we search for files in the list
 * with the given fid.
 * */
static int alloc_fid(void)
{
	int fid = 2;
	while(findFdElem(fid)!=NULL)
		fid++;
	return fid;
}
/*
static mapid_t mmap (int fd, void *addr)
{
	struct file *fileStruct;
	struct file *reopenedFile;
	struct thread *t = thread_current();
	int length = 0;
	int i = 0;
	
	if(fd < 2)
		return -1;
	
	if(addr == NULL || addr = 0x0 || pg_ofs(addr) != 0)
		return -1;
		
	if((fileStruct = findFile(fd)) == NULL)
		return -1;
	
	if((length = file_length(fileStruct)) <= 0)
		return -1;
	
	for(i = 0; i < offset; i+=PGSIZE)
		if(get_supptable_page(&t->suppl_page_table, addr + i) || pagedir_get_page(t->pagedir, addr + i))
			return -1;
	lock_acquire(&fileLock);
	reopenedFile = file_reopen(fileStruct);
	lock_release(&fileLock);
	
	if(reopenedFile == NULL)
		return -1;
	// call insert mmfiles function (to be created in process.c)
}*/
/*
static void munmap (mapid_t){
	
}
*/
/* Function for finding the file in a fd list given a fd.
 * Starts by finding the fd element in the list for the given fd
 * and returns the file associated with it.
 * */
static struct file * findFile (int fd)
{
	struct fd_elements *ret;
	ret = findFdElem (fd);
	if (!ret)
		return NULL;
	return ret->File;
}

/* Function for finding the fd element in the fd list given a fd.
 * Scans the list and compare the fd of the element in the list
 * with the fd given and in case it maches, returns that fd element
 * If there is no such fd element, return null.
 * */
static struct fd_elements *findFdElem (int fd)
{
	struct fd_elements *ret;
	struct list_elem *l;
	for (l = list_begin (&fileList); l != list_end (&fileList); l = list_next (l))
	{
		ret = list_entry (l, struct fd_elements, elem);
		if (ret->fd == fd)
			return ret;
	}
	return NULL;
}

/* This function finds the element for a given fd in in the list
 * of opened files for a given thread. It gets the current thread.
 * Scans the list and compare the fd of the element in the list with
 * the fd given and in case it maches, it returns that fd element.
 * If there is no such fd element, it returns NULL. 
 * */
static struct fd_elements *findFdElemProcess (int fd) {
	struct fd_elements *ret;
	struct list_elem *l;
	struct thread *t;
	t = thread_current ();
	for (l = list_begin (&t->files); l != list_end (&t->files); l = list_next (l))
	{
		ret = list_entry (l, struct fd_elements, thread_elem);
		if (ret->fd == fd)
			return ret;
	}
	return NULL;
}

/* This function has been used to check strings in
 * open,exec,create,remove system calls
 * Takes an index position of string and checks its
 * range in the user address space page by page.
 * */
static void string_check_terminate(char *str)
{
	  char* temp = str;
	  unsigned ptr;
	  while(*temp)
	  {
    	for(ptr = (unsigned)temp; ptr < (unsigned)(temp+1);
    		ptr = ptr + (PGSIZE - ptr % PGSIZE));
    	if(!is_user_vaddr((void *)ptr))
    		exit(-1);
	    ++temp;
	  }
}

/* This function has been used to check the address of the buffer
 * indices in read,write system call.It scans the address index of
 * the buffer at page size offsets.It advances on the basis of buffer
 * size.If the buffer size is zero then it terminates the loop. If the
 * buffer size is greater than 1 page then it advances by decrementing
 * buffer size by 1 page size.And if the size of buffer is less than 1
 * page then it just jumps to the end of the buffer.
 * */
static void buffer_check_terminate(void *buffer, unsigned size)
{
	unsigned buffer_size = size;
	void *buffer_tmp ;
	buffer_tmp= buffer;
	while(buffer_tmp != NULL)
	{
	      if (!is_user_vaddr(buffer_tmp) || buffer_tmp==NULL)
		  exit (-1);

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
}

/* Function to kill the current thread
 * */
void terminate_process(void)
{
	DPRINTF("TERMINATE_PROCESS");
	exit(-1);
}

/* Verifies the addresses passed to it by checking if they are
 * not NULL as well as not lying in the kernel space above PHYS_BASE.
 * Then it uses get_user() to get the calue stored at the passed
 * address. */
static int
get_valid_val(int *uaddr)
{
  if(!is_user_vaddr(uaddr) || uaddr==NULL )
      return -1;
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
