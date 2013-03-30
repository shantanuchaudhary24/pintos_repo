#include <stdio.h>
#include "lib/kernel/hash.h"
#include "filesys/file.h"
#include "threads/thread.h"

/* Determining the type of page of supplementary page */
#define SWAP 001
#define FILE 002
#define MMF  004

/* Defines the size of per process stack*/
#define STACK_SIZE (8*(1<<20))

/* Supplementary page table management structs and function definitions */
struct supptable_page
{
	/* For Hashing */
	struct hash_elem hash_index;

	/* Data to be fed into the supplementary page table*/
	int page_type;
	struct file *file;
	off_t offset;
	void *uvaddr;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
	bool is_page_loaded;				// whether page is loaded or not	

	/* For Swapping purpose*/
	size_t swap_slot_id;
	bool swap_writable;
};
bool init_supptable(struct hash *table);
bool supptable_add_page(struct hash *table,struct supptable_page *page_entry);
bool supptable_add_file(int type,struct file *file, off_t ofs, uint8_t *upage,uint32_t read_bytes, uint32_t zero_bytes, bool writable);
struct supptable_page *get_supptable_page(struct hash *table, void *vaddr);
void free_supptable(struct hash *table);
void write_page_to_file(struct supptable_page *page_entry);
void grow_stack(void *vaddr);
bool load_supptable_page(struct supptable_page *page_entry);

