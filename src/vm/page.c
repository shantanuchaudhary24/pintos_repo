#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "lib/kernel/hash.h"
#include "lib/debug.h"
#include "lib/stdbool.h"
#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/frame.h"
#include "vm/debug.h"
#include "vm/page.h"

/* Filesystem Lock*/
extern struct lock filesys_lock;

static bool load_page_swap(struct supptable_page *page_entry);
static bool load_page_file(struct supptable_page *page_entry);
static bool load_page_mmf (struct supptable_page *page_entry);
static void supptable_free_page(struct hash_elem *element, void *aux UNUSED);
static unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,void *aux UNUSED);

static bool load_file(struct supptable_page *page_entry);
static bool set_page_suppl(struct supptable_page *page_entry, void *kpage,struct thread *t);
/* This function initialises the supplementary table
 * by call hash_init which is complimented by the use of
 * page_has and page_less functions.It returns true on success
 * and false on failure to create the table
 * */
bool init_supptable(struct hash *table)
{
	return hash_init(table,page_hash,page_less,NULL);
}

/* Function to add page to suppl. table by checking if pointer 
 * to the table or page_entry passed to it isn't NULL. It then uses
 * hash_insert function to insert the entry in the hash table and returns
 * result appropriately.
 * */
bool supptable_add_page(struct hash *table,struct supptable_page *page_entry)
{
	struct hash_elem *temp;
	if(table==NULL || page_entry==NULL)
		return false;
	temp=hash_insert(table,&page_entry->hash_index);
	if(temp==NULL)
		return true;
	else return false;
}

/* This function adds the file data into the supplementary page
 * table.It determines the type of file(namely FILE,MMF,SWAP)
 * and adds it to the database of suppl. page table using hash_insert
 * function.The function returns true if it is able to insert the file
 * data into the table.
 * */
bool supptable_add_file(int type,struct file *file, off_t ofs, uint8_t *upage,uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	struct thread *t=thread_current();
	struct supptable_page *page_entry;
	page_entry=malloc(sizeof(struct supptable_page));
	if(file==NULL || upage==NULL || page_entry==NULL)
		return false;

	page_entry->page_type=type;
	page_entry->file=file;
	page_entry->offset=ofs;
	page_entry->uvaddr=upage;
	page_entry->read_bytes=read_bytes;
	page_entry->zero_bytes=zero_bytes;
	page_entry->writable=writable;
	page_entry->is_page_loaded=false;

	DPRINT_PAGE("supptable_add_file:ENTRY ADDR:%x\n",(uint32_t)(page_entry->uvaddr));

	if(hash_insert(&t->suppl_page_table,&page_entry->hash_index)==NULL)
	{
		DPRINTF_PAGE("supptable_add_file:PAGE ADDED.\n");
		return true;
	}
	else
	{
		DPRINTF_PAGE("supptable_add_file:PAGE ALREADY EXISTS");
		return false;
	}
}

/* Function requires when a dirty pagedir is encountered.
 * It just seeks to the required offset and writes the contents of
 * the address to the file passes to it from the suppl page table
 * entry.
 * */
void write_page_to_file(struct supptable_page *page_entry)
{
	if(page_entry->page_type & MMF)
	{
		lock_acquire(&filesys_lock);
		file_seek(page_entry->file,page_entry->offset);
		file_write(page_entry->file,page_entry->uvaddr,page_entry->read_bytes);
		lock_release(&filesys_lock);
	}
}

/* This function fetches a page from the table which is passed to it by reference
 * using the vaddr.It uses the hash_find function to find the element in the table.
 * It returns the corresponding page on successfully finding it else it returns
 * NULL.
 * */
struct supptable_page *get_supptable_page(struct hash *table, void *vaddr)
{
	struct hash_elem *temp_hash_elem;
	struct supptable_page page_entry;

	page_entry.uvaddr=vaddr;
	temp_hash_elem=hash_find(table,&page_entry.hash_index);

	if(temp_hash_elem!=NULL)
	{
		DPRINT_PAGE("get_supptable_page:RETURN PAGE->VADDR:%x\n",(uint32_t)vaddr);
		return hash_entry(temp_hash_elem,struct supptable_page,hash_index);
	}
	else
	{
		DPRINTF_PAGE("get_supptable_page:NULL RETURN\n");
		return NULL;
	}
}

/* Destroys the supplementary table. When a thread exits this function is
 * called in order to desroy its supplementary table.This function uses
 * hash_destroy and an auxiliary function(defined below) to free the suppl.table
 * */
void free_supptable(struct hash *table)
{
	hash_destroy(table,supptable_free_page);
}

/* This is the auxiliary function that is called upon each and every entry
 * of the hash table in order to free the resources captured by it.
 * The function chooses the element based upon the hash_element passed to it
 * by using hash_entry function. It also frees the page from swap disk if
 * the page belongs to the swap space.
 * */
static void supptable_free_page(struct hash_elem *element, void *aux UNUSED)
{
	struct supptable_page *temp_page;
	temp_page=hash_entry(element,struct supptable_page, hash_index);
	if(temp_page->page_type & SWAP)
		swap_clear_slot(temp_page->swap_slot_index);// clear swap slot too if occupied any but in case of sharing this will not be removed
	free(temp_page);
}

/* This function is used to grow the stack of a user process when we are
 * accessing an address which is not the valid bounds of the stack of the
 * process.It works by allocating a frame corresponding to the vaddr
 * passed to it.It then sets the page corresponding to the frame.Returns
 * false on failure and frees the alloted frame.
 * */
void grow_stack(void *vaddr)
{
	struct thread *t=thread_current();
	void *temp_frame;
	temp_frame= allocateFrame(PAL_USER|PAL_ZERO,vaddr);
	if(temp_frame==NULL)
		PANIC("Frame allocation failed");
	else
	{
		if(!pagedir_set_page(t->pagedir, pg_round_down(vaddr), temp_frame, true))
			freeFrame(vaddr);
	}
}

/* Given a page entry load it to the appropriate
 * appratus.This function first checks the type of page
 * passed to it and then calls the corresponding functions.
 * Returns true on success and false on failure.
 * */
bool load_supptable_page(struct supptable_page *page_entry)
{
	bool result=false;
	switch(page_entry->page_type)
	{
		case FILE:
			DPRINTF_PAGE("load_supptable_page:LOAD_PAGE_FILE\n");
			result=load_file(page_entry);
			break;
		case MMF | SWAP:
		case MMF:
			DPRINTF_PAGE("load_supptable_page:LOAD_PAGE_MMF\n");
			result=load_file(page_entry);
			break;
		case FILE | SWAP:
		case SWAP:
			DPRINTF_PAGE("load_supptable_page:LOAD_PAGE_SWAP\n");
			result=load_page_swap(page_entry);
			break;
	}
	return result;
}

/* Extension of load_supptable_page function to load a page
 * from disk space into the memory page table.Allocates a frame
 * based upon the page type.Frees it if the mapping fails.
 * It then loads the EXECUTABLE file into the frame specified by kpage.
 * It then zeroes out memory at kpage + the supplementary
 * page read_bytes. Then it ,adds the page to the process's address
 * space. If the load is successful this function returns true
 * else returns false.Depending upon the bits set in page_type we mark
 * the page to be in FILE/MMF.
 * */
static bool load_file(struct supptable_page *page_entry)
{
	DPRINT_PAGE("load_page_file:PAGE UVADDR:%x\n",(uint32_t)page_entry->uvaddr);
	void *kpage;
	int ret=0;
	struct thread *t=thread_current();

	lock_acquire(&filesys_lock);
	file_seek (page_entry->file, page_entry->offset);
	kpage= allocateFrame(PAL_USER,page_entry->uvaddr);
	if (kpage == NULL)
	{
		DPRINTF_PAGE("load_page_file:FRAME ALLOC. FAILED\n");
		lock_release(&filesys_lock);
		return false;
	}

	ret=file_read(page_entry->file,kpage,page_entry->read_bytes);
	lock_release(&filesys_lock);
	if (ret!= (int)page_entry->read_bytes)
	{
		DPRINTF_PAGE("load_page_file:FILE_READ FAIL:");
		freeFrame(kpage);
	    return false;
	}

	memset(kpage + page_entry->read_bytes, 0,page_entry->zero_bytes);

	return set_page_suppl(page_entry,kpage,t);
}


static bool set_page_suppl(struct supptable_page *page_entry, void *kpage,struct thread *t)
{
	if((page_entry->page_type & (MMF|SWAP)) || (page_entry->page_type & MMF))
	{
		if (!pagedir_set_page (t->pagedir, page_entry->uvaddr, kpage, true))
		{
			freeFrame (kpage);
			DPRINTF_PAGE("Load Page MMF: false : couldn't set pagedir entry\n");
			return false;
		}

		page_entry->is_page_loaded = true;
		if (page_entry->page_type & SWAP)
			page_entry->page_type = MMF;
		return true;
	}

	if(page_entry->page_type & FILE)
	{
		if (!pagedir_set_page (t->pagedir, page_entry->uvaddr, kpage,page_entry->writable))
		{
			DPRINTF_PAGE("load_page_file:FREE FRAME");
			freeFrame (kpage);
			return false;
		}
		page_entry->is_page_loaded = true;
		return true;
	}
	PANIC("set_page");
	return false;
}

/* Extension of load_supptable_page function to load a page
 * from swap space into the page table. Allocates a frame corresponding
 * the address passed to it.Maps the frame to the page directory.
 * Frees it if the mapping fails.After swap in, if the page
 * was a swap page, we remove the corresponding entry from suppl
 * page table. If it is on the disk then we set the page type
 * and leave it in the suppl. table. If the load is successful
 * this function returns true else returns false
 * */
static bool load_page_swap(struct supptable_page *page_entry)
{
	struct thread *t=thread_current();
	void *kpage;
	kpage = allocateFrame(PAL_USER,page_entry->uvaddr);
	if (kpage == NULL)
		return false;

	if (!pagedir_set_page (t->pagedir, page_entry->uvaddr, kpage,page_entry->swap_writable))
	{
		freeFrame (kpage);
		return false;
	}

	swap_in_page(page_entry->swap_slot_index, page_entry->uvaddr);
    if (page_entry->page_type & SWAP)
    	hash_delete (&t->suppl_page_table, &page_entry->hash_index);

	if (page_entry->page_type & (FILE | SWAP))
	{
		page_entry->page_type = FILE;
		page_entry->is_page_loaded = true;
	}
	return true;
}

/* Extension of load_supptable_page function to load a page
 * from disk space into the memory page table.Allocates a frame
 * based upon the page type.Frees it if the mapping fails.
 * It then loads the EXECUTABLE file into the frame specified by kpage.
 * It then zeroes out memory at kpage + the supplementary
 * page read_bytes. Then it ,adds the page to the process's address
 * space. If the load is successful this function returns true
 * else returns false.
 * */
static bool load_page_file(struct supptable_page *page_entry)
{
	DPRINT_PAGE("load_page_file:PAGE UVADDR:%x\n",(uint32_t)page_entry->uvaddr);
	void *kpage;
	int ret=0;
	struct thread *t=thread_current();

	lock_acquire(&filesys_lock);
	file_seek (page_entry->file, page_entry->offset);
	kpage= allocateFrame(PAL_USER,page_entry->uvaddr);
	if (kpage == NULL)
	{
		DPRINTF_PAGE("load_page_file:FRAME ALLOC. FAILED\n");
		lock_release(&filesys_lock);
		return false;
	}

	ret=file_read(page_entry->file,kpage,page_entry->read_bytes);
	lock_release(&filesys_lock);

	if (ret!= (int)page_entry->read_bytes)
	{
		DPRINTF_PAGE("load_page_file:FILE_READ FAIL:");
		freeFrame(kpage);
	    return false;
	}

	memset(kpage + page_entry->read_bytes, 0,page_entry->zero_bytes);

	if (!pagedir_set_page (t->pagedir, page_entry->uvaddr, kpage,page_entry->writable))
	{
		DPRINTF_PAGE("load_page_file:FREE FRAME");
		freeFrame (kpage);
		return false;
	}
	page_entry->is_page_loaded = true;
	DPRINTF_PAGE("load_page_file:TRUE\n");
	return true;
}

/* Extension of load_supptable_page function to load a memory
 * mapped file into the memory frame table.Allocates a frame
 * based upon the page type.Frees the frame if the page read.
 * It then loads the file from
 * disk into the memory page table.It then zeroes out the suppl
 * page read_bytes. Then it ,adds the page to the process's address
 * space. If the load is successful this function returns true
 * else returns false.
 * */
static bool load_page_mmf (struct supptable_page *page_entry)
{
	void *kpage ;
	int ret=0 ;
	struct thread *cur = thread_current ();

	lock_acquire(&filesys_lock);
	file_seek (page_entry->file, page_entry->offset);
	kpage = allocateFrame (PAL_USER, page_entry->uvaddr);
	if (kpage == NULL)
	{
		DPRINTF_PAGE("Load Page MMF: false : couldn't allocate Frame\n");
		lock_release(&filesys_lock);
		return false;
	}

	ret=file_read (page_entry->file, kpage, page_entry->read_bytes);
	lock_release(&filesys_lock);
	if ( ret!= (int) page_entry->read_bytes)
	{
		freeFrame (kpage);
		DPRINTF_PAGE("Load Page MMF: false : couldn't read file\n");
		return false; 
    }
	
	memset (kpage + page_entry->read_bytes, 0, PGSIZE - page_entry->read_bytes);

	if (!pagedir_set_page (cur->pagedir, page_entry->uvaddr, kpage, true))
	{
		freeFrame (kpage);
		DPRINTF_PAGE("Load Page MMF: false : couldn't set pagedir entry\n");
		return false; 
    }

	page_entry->is_page_loaded = true;
	if (page_entry->page_type & SWAP)
		page_entry->page_type = MMF;
	DPRINTF_PAGE("Load Page MMF: true\n");
	return true;
}


/* Auxiliary Functions for hashing */
/* Returns a hash value for page p. */
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
	const struct supptable_page *p = hash_entry (p_, struct supptable_page, hash_index);
	return hash_bytes (&p->uvaddr, sizeof p->uvaddr);
}

/* Returns true if page a precedes page b. */
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_,void *aux UNUSED)
{
	const struct supptable_page *a = hash_entry (a_, struct supptable_page, hash_index);
	const struct supptable_page *b = hash_entry (b_, struct supptable_page, hash_index);
	return a->uvaddr < b->uvaddr;
}
