#include "lib/kernel/hash.h"

struct supptable_page
{
		struct hash_elem hash_index;
		struct file *file;
		off_t ofs;
		uint8_t *uaddr;
		uint32_t read_bytes;
		uint32_t zero_bytes;
        bool writable;	
}
