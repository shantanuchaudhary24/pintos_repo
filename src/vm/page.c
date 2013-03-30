#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "lib/stdbool.h"
#include "vm/swap.h"
#include "vm/frame.h"

bool init_supptable(struct hash *table);
bool supptable_add_page(struct hash *table,struct supptable_page *page_entry);
bool supptable_add_file(int type,struct file *file, off_t ofs, uint8_t *upage,uint32_t read_bytes, uint32_t zero_bytes, bool writable);
struct supptable_page *get_supptable_page(struct hash *table, void *vaddr);
void free_supptable(struct hash *table);
void write_page_to_file(struct supptable_page *page_entry);
void grow_stack(void *vaddr);
bool load_supptable_page(struct supptable_page *page_entry);

static bool load_to_swap(struct supptable_page *page_entry);
static bool load_to_file(struct supptable_page *page_entry);
static void supptable_free_page(struct hash_elem *element, void *aux UNUSED);
static unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,void *aux UNUSED);

// Function to initialize supplementary table
bool init_supptable(struct hash *table)
{
	if(hash_init(table,page_hash,page_less,NULL))
		return true;
	else return false;	
}

/* Function to add page to suppl. table by checking if pointer 
 * to the table or page_entry passed to it isn't NULL. It then uses
 * hash functions to insert the entry in the hash table and returns
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

// Function to add page to supplementary table
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
	
	if(hash_insert(&t->suppl_page_table,&page_entry->hash_index)==NULL)
		return true;
	else {
		printf("page_entry already exists in the table");		// Do something else
		return false;
	}
}

// If page is dirty
void write_page_to_file(struct supptable_page *page_entry)
{
	if(page_entry->page_type==MMF)
	{
		file_seek(page_entry->file,page_entry->offset);
		file_write(page_entry->file,page_entry->uvaddr,page_entry->read_bytes);
	}
}

struct supptable_page *get_supptable_page(struct hash *table, void *vaddr)
{
	struct hash_elem *temp_hash_elem;
	struct supptable_page page_entry;
	page_entry.uvaddr=vaddr;
	temp_hash_elem=hash_find(table,&page_entry.hash_index);
	if(temp_hash_elem!=NULL)
		return hash_entry(temp_hash_elem,struct supptable_page,hash_index);
	else return NULL;	
}

void free_supptable(struct hash *table)
{
	hash_destroy(table,supptable_free_page);
}

static void supptable_free_page(struct hash_elem *element, void *aux UNUSED)
{
	struct supptable_page *temp_page;
	temp_page=hash_entry(element,struct supptable_page, hash_index);
	if(temp_page->page_type & SWAP)
		swap_clear_slot(temp_page->swap_slot_id);// clear swap slot too if occupied any but in case of sharing this will not be removed
	free(temp_page);
}

void grow_stack(void *vaddr)
{
	struct thread *t=thread_current();
	void *temp_page;
	temp_page= allocateFrame(PAL_USER|PAL_ZERO,vaddr);
	if(temp_page==NULL)
		PANIC("Frame allocation failed");
	else
	{
		if(!pagedir_set_page(t->pagedir, pg_round_down(vaddr), temp_page, true))
			freeFrame(temp_page);
	}
}

/* Given a page entry load it to the appropriate
 * structure.
 *
 * */
bool load_supptable_page(struct supptable_page *page_entry)
{
	bool result=false;
	switch(page_entry->page_type)
	{
		case FILE:
			result=load_to_file(page_entry);
			break;
		case MMF:
			//result=load_to_mmf(page_entry);
			PANIC("MMF not implemented");
			break;
		case SWAP:
			result=load_to_swap(page_entry);
			break;
	}
	return result;
}

static bool load_to_swap(struct supptable_page *page_entry)
{
	struct thread *t=thread_current();
	/* Get a page of memory. */
	void *kpage;
	kpage = allocateFrame(PAL_USER,page_entry->uvaddr);
	if (kpage == NULL)
		return false;

	/* Map the user page to given frame */
	if (!pagedir_set_page (t->pagedir, page_entry->uvaddr, kpage,page_entry->swap_writable))
	{
		freeFrame (kpage);
		return false;
	}

	/* Swap data from disk into memory page */
	swap_in_page(page_entry->swap_slot_id, page_entry->uvaddr);
    if (page_entry->page_type == SWAP)
	{
    	/* After swap in, remove the corresponding entry in suppl page table */
    	hash_delete (&t->suppl_page_table, &page_entry->hash_index);
	}
	if (page_entry->page_type == (FILE | SWAP))
	{
		page_entry->page_type = FILE;
		page_entry->is_page_loaded = true;
	}
	return true;
}


static bool load_to_file(struct supptable_page *page_entry)
{
	struct thread *t=thread_current();
	file_seek (page_entry->file, page_entry->offset);

	/* Get a page of memory. */
	void *kpage = allocateFrame(PAL_USER,page_entry->uvaddr);
	if (kpage == NULL)
		return false;

	/* Load this page. */
	if (file_read (page_entry->file, kpage,page_entry->read_bytes)!= (int)page_entry->read_bytes)
	{
		freeFrame(kpage);
	    return false;
	}

	memset(kpage + page_entry->read_bytes, 0,page_entry->zero_bytes);

	/* Add the page to the process's address space. */
	if (!pagedir_set_page (t->pagedir, page_entry->uvaddr, kpage,page_entry->writable))
	{
		freeFrame (kpage);
	    return false;
	}
	page_entry->is_page_loaded = true;
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
