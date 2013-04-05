#include "lib/stdbool.h"
#include "threads/thread.h"


struct mmfStruct {
	mapid_t mapid;
	struct file *file;
	void * begin_addr;
	unsigned pageCount;
	struct hash_elem Element;
};

bool mmfiles_init(struct hash *mmfiles);
mapid_t mmfiles_insert (void *, struct file*, int32_t);
void free_mmfiles (struct hash *mmfiles);
void mmfiles_remove (mapid_t);
