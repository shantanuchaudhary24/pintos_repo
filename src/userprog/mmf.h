#include "userprog/syscall.h"
#include "threads/thread.h"

struct mmfStruct {
	mapid_t mapid;
	struct file *file;
	void * begin_addr;
	unsigned pageCount;
	struct hash_elem Element;
};

unsigned mmfile_hash (const struct hash_elem *a, void *aux UNUSED);
bool mmfile_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
bool mmfiles_init(struct hash *mmfiles);
mapid_t mmfiles_insert (void *, struct file*, int32_t);
void mmfiles_free_entry (struct mmfStruct* mmfile);
void free_mmfiles (struct hash *mmfiles);
void mmfiles_remove (mapid_t);
