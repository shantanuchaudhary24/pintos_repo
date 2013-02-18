#include "userprog/syscall.h"
//14406
#include <stdio.h>
#include <syscall-nr.h>				// reference to library to get interrupt numbers
#include "threads/interrupt.h"		// reference to library for intr_frame 
#include "threads/thread.h"			

static void syscall_handler (struct intr_frame *);
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit (); 
}
