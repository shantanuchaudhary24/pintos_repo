#include "lib/stdbool.h"
#include "threads/thread.h"

/*struct for storing details about a single mapping*/
struct mmfStruct {
	mapid_t mapid;
	struct file *file;
	void *begin_addr;
	unsigned pageCount;
	struct hash_elem Element;
};

bool mmfiles_init(struct hash *mmfiles);
mapid_t mmfiles_insert (void *addr, struct file* file, int32_t len);
void free_mmfiles (struct hash *mmfiles);
void mmfiles_remove (mapid_t mapid);
