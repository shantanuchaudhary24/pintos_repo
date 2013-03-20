#include <stdio.h>
#include "vm/page.h"
#include "lib/kernel/hash.h"

bool supptable_add_page(struct file *file, off_t ofs, uint8_t *upage,uint32_t read_bytes, uint32_t zero_bytes, bool writable);

bool supptable_del_page(struct thread *t);

struct page *page_lookup (const void *address);

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
	else return false;
}

bool supptable_del_page(struct thread *t)
{
	//if(hash_delete(&t->suppl_table,))
	return false;
}
// Function to delete page

 	
/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct page *
page_lookup (const void *address)
{
  struct page p;
  struct hash_elem *e;

  p.addr = address;
  e = hash_find (&pages, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->addr < b->addr;
}
