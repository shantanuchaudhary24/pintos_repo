#include <stdio.h>
#include "lib/kernel/hash.h"
#include "filesys/file.h"

struct supptable_page
{
		struct hash_elem hash_index;
		struct file *file;
		off_t ofs;
		uint8_t *uaddr;
		uint32_t read_bytes;
		uint32_t zero_bytes;
        bool writable;	
};
bool init_supptable(struct hash *table);

bool supptable_add_page(struct file *file, off_t ofs, uint8_t *upage,uint32_t read_bytes, uint32_t zero_bytes, bool writable);

bool supptable_del_page(struct hash *table,struct hash_elem element);
