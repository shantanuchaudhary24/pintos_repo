#include "userprog/exception.h"
//13018
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/debug.h"
#include "userprog/pagedir.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      thread_current()->exit_status=-1;
      process_terminate();
      break;

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
        if((((uint32_t)f->eax-(uint32_t)f->eip)==2) || (((uint32_t)f->eax-(uint32_t)f->eip)==3))
        {
        	f->eip=(void *)f->eax;
        	f->eax=(-1);
        }else
        {
        	PANIC("UNEXPECTED BUG IN KERNEL");
        }
        break;
        
    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
      break;
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write UNUSED; /* True: access was write, false: access was read. */
  bool user UNUSED;  /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */
  struct thread *t=thread_current();
  struct supptable_page *page_entry;

  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  intr_enable ();

  page_fault_cnt++;

  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;
  DPRINT_EXCEP("page_fault:not_present:%d\n",not_present);
  DPRINT_EXCEP("page_fault:write:%d\n",write);
  DPRINT_EXCEP("page_fault:user:%d\n",user);

    	  	  	  	  	  	  /*  PAGE FAULT HANDLING*/

  switch(f->cs)
  {
  	  /* For user stack segment, we check for write violation,NULL address, address
  	   * less than PHYSBASE.If there is any violation, we terminate the process.Next
  	   * we consult the supplementary page table to fetch the appropriate page
  	   * corresponding to the fault address.We load the entry corresponding to the
  	   * page(depending upon swap,file,mmf as needed).Otherwise we check for read/write
  	   * access and check if stack needs to be grown.In the final case we set the page
  	   * in the page directory or terminate if it fails.
  	   * */
	  case SEL_UCSEG:
  		  DPRINT_EXCEP("page_fault:UCSEG:fault_addr:%x\n",(uint32_t)fault_addr);

  		  if(!not_present || fault_addr==NULL || !is_user_vaddr(fault_addr))
  		  {
  			  DPRINTF_EXCEP("page_fault:UCSEG:WRITE VIOLATION/NULL/VALID_USER_VADDR\n");
  			  t->exit_status=-1;
  			  process_terminate();

  		  }
  		  else
  		  {
  			  page_entry=get_supptable_page(&t->suppl_page_table,pg_round_down(fault_addr));
  			  DPRINT_EXCEP("page_fault:UCSEG:Getting correct Entry Addr:%x\n",(uint32_t)page_entry->uvaddr);
  			  if(page_entry!=NULL && !page_entry->is_page_loaded)
  			  {
  				  DPRINTF_EXCEP("page_fault:UCSEG:LOAD PAGE\n");
  				  load_supptable_page(page_entry);
  			  }
  			  else if(page_entry==NULL && ((int *)fault_addr>=(int *)(f->esp)-8) && (pg_round_down(f->esp)>=(PHYS_BASE-STACK_SIZE)))
  			  {
  				  DPRINTF_EXCEP("page_fault:UCSEG:GROW STACK\n");
  				  grow_stack(fault_addr);
  			  }
  			  else kill(f);
  		  }
  		  break;

  	  /* For kernel stack segment, we check for write violation,NULL address, address
  	   * less than PHYSBASE and saved stack pointer is in valid user virtual address space.
  	   * If there is any violation, we terminate the process.Next
  	   * we consult the supplementary page table to fetch the appropriate page
  	   * corresponding to the fault address.We load the entry corresponding to the
  	   * page(depending upon swap,file,mmf as needed).Otherwise we check for read/write
  	   * access and check if stack needs to be grown(using the saved stack pointer).In
  	   * the final case we set the page in the page directory or terminate if it fails.
  	   * */
	  case SEL_KCSEG:
  		  DPRINT_EXCEP("page_fault:KERNEL fault_addr:%x\n",(uint32_t)fault_addr);
  		  DPRINT_EXCEP("page_fault:KERNEL t->stack addr:%x\n",(uint32_t)t->stack);
  		  DPRINT_EXCEP("is_user_vaddr(fault_addr):%d\n",is_user_vaddr(fault_addr));
  		  DPRINT_EXCEP("EIP is:%x\n",(void *)f->eip);
  		  if(!not_present || fault_addr==NULL || !is_user_vaddr(fault_addr))
  		  {
  			  t->exit_status=-1;
  			  process_terminate();
  		  }
  		  else
  		  {
  			  page_entry=get_supptable_page(&t->suppl_page_table,pg_round_down(fault_addr));
  			  if(page_entry!=NULL && !page_entry->is_page_loaded)
  			  {
  				  DPRINTF_EXCEP("page_fault:KCSEG:LOAD PAGE\n");
  				  load_supptable_page(page_entry);
  			  }
  			  else if(page_entry == NULL && ((int *)fault_addr>=((int *)(t->stack))-8) && (pg_round_down(t->stack)>=(PHYS_BASE-STACK_SIZE)))
  			  {
  				  DPRINTF_EXCEP("page_fault:KCSEG:GROW STACK\n");
  				  grow_stack(fault_addr);
  			  }
  			  else kill(f);
  		  }break;
  	}
  //kill(f);
}
