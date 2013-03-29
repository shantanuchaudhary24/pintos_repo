#include <stdio.h>
#include "lib/kernel/hash.h"
#include "filesys/file.h"

// Supplementary page table management structs and function definitions
#define SWAP 001
#define FILE 002
#define MMF  004

struct supptable_page
{
	/* For Hashing*/
	struct hash_elem hash_index;

	/* Data to be fed into the supplementary page table*/
	int page_type;
	struct file *file;
	off_t offset;
	void *vaddr;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
	bool is_page_loaded;				// whether page is loaded or not	

	/* For Swapping purpose*/
	size_t swap_slot_id;
	bool swap_writable;
};
bool init_supptable(struct hash *table);
bool supptable_add_page(struct hash *table,struct supptable_page *page);
bool supptable_add_file(struct file *file, off_t ofs, uint8_t *upage,uint32_t read_bytes, uint32_t zero_bytes, bool writable);
struct supptable_page *get_supptable_page(struct hash *table, void *vaddr);
void free_supptable(struct hash *table);
void grow_stack(void *vaddr);

//bool supptable_add_mmf(struct file *file, off_t ofs, uint8_t *upage,uint32_t read_bytes, uint32_t zero_bytes, bool writable);
