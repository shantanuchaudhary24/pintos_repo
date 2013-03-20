#include <stdio.h>
#include <stddef.h>
#include "vm/page.h"
#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/malloc.h"

bool init_supptable(struct hash *table);

bool supptable_add_page(struct file *file, off_t ofs, uint8_t *upage,uint32_t read_bytes, uint32_t zero_bytes, bool writable);

bool supptable_del_page(struct hash *table,struct hash_elem element);

static unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);

static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,void *aux UNUSED);

// Function to initialize supplementary table
bool init_supptable(struct hash *table)
{
	if(hash_init(table,page_hash,page_less,NULL))
		return true;
	else return false;	
}

// Function to add page to supplementary table
bool supptable_add_page(struct file *file, off_t ofs, uint8_t *upage,uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	struct thread *t=thread_current();
	struct supptable_page *page;
	page=malloc(sizeof(struct supptable_page));
	if(file==NULL || upage==NULL || page==NULL)
		return false;
	page->file=file;
	page->ofs=ofs;
	page->uaddr=upage;
	page->read_bytes=read_bytes;
	page->zero_bytes=zero_bytes;
	page->writable=writable;
	
	if(hash_insert(&t->suppl_table,&page->hash_index)==NULL)
		return true;
	else {
		printf("Page already exists in the table");		// Do something else
		return false;
	}
}

bool supptable_del_page(struct hash *table,struct hash_elem element)
{
	//if(hash_delete(&t->suppl_table,))
	return false;
}
// Function to delete page

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct supptable_page *p = hash_entry (p_, struct supptable_page, hash_index);
  return hash_bytes (&p->uaddr, sizeof p->uaddr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct supptable_page *a = hash_entry (a_, struct supptable_page, hash_index);
  const struct supptable_page *b = hash_entry (b_, struct supptable_page, hash_index);

  return a->uaddr < b->uaddr;
}
