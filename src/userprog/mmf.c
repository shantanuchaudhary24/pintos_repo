#include "userprog/mmf.h"
#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "vm/debug.h"

static void free_mmfiles_entry (struct hash_elem *e, void *aux UNUSED);

bool mmfiles_init(struct hash *mmfiles){
	return hash_init (mmfiles, mmfile_hash, mmfile_less, NULL);
}

mapid_t mmfiles_insert (void *addr, struct file* file, int32_t len){
	struct thread *t = thread_current ();
	struct mmfStruct *mmf;
	struct hash_elem *result;
	int i = 0;
	
	if ((mmf = calloc (1, sizeof *mmf)) == NULL)
		return -1;

	mmf->mapid = t->mapid_allocator++;
	mmf->file = file;
	mmf->begin_addr = addr;
	
	for(;len > 0; len-=PGSIZE){
		if(!supptable_add_file(MMF, file, ((i)*PGSIZE), (addr + (i)*PGSIZE), (len < PGSIZE ? len : PGSIZE), 0, NULL))
			return -1;
		i++;
	}

	mmf->pageCount = i;
	result = hash_insert (&t->mmfiles, &mmf->Element);

	return (result!=NULL) ? -1 : mmf->mapid;
}

void mmfiles_remove (mapid_t mapid) {
	struct thread *t = thread_current ();
	struct mmfStruct mmf;
	struct mmfStruct *mmf_ptr;
	struct hash_elem *hashElement;
	DPRINTF_MMF("mmfiles_remove: begin\n");
	mmf.mapid = mapid;
	hashElement = hash_delete (&t->mmfiles, &mmf.Element);
	if (hashElement != NULL){
		mmf_ptr = hash_entry (hashElement, struct mmfStruct, Element);
		mmfiles_free_entry (mmf_ptr);
	}
	return;
}

void mmfiles_free_entry (struct mmfStruct* mmfile){
	struct thread *t = thread_current ();
	struct hash_elem *hashElement;
	struct supptable_page spte;
	struct supptable_page *spte_ptr;
	uint32_t i = 0;
	DPRINTF_MMF("mmfiles_free_entry: begin\n");
	for(i = 0; i < mmfile->pageCount; i++){
		spte.uvaddr = mmfile->begin_addr + i*PGSIZE;
		hashElement = hash_delete (&t->suppl_page_table, &spte.hash_index);
		
		if(hashElement != NULL){
			spte_ptr = hash_entry (hashElement, struct supptable_page, hash_index);
		
			if(spte_ptr->is_page_loaded && pagedir_is_dirty(t->pagedir, spte_ptr->uvaddr)) {
				lock_acquire(&fileLock);
				file_seek(spte_ptr->file, spte_ptr->offset);
				file_write(spte_ptr->file, spte_ptr->uvaddr, spte_ptr->read_bytes);
				lock_release(&fileLock);
			}
			free (spte_ptr);
		}
	}
	DPRINTF_MMF("mmfiles_free_entry: before close file\n");
	lock_acquire (&fileLock);
	file_close (mmfile->file);
	lock_release (&fileLock);

	free (mmfile);
}

unsigned mmfile_hash (const struct hash_elem *a, void *aux UNUSED) {
  const struct mmfStruct *b = hash_entry (a, struct mmfStruct, Element);
  return hash_bytes (&b->mapid, sizeof(b->mapid));
}


bool mmfile_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  const struct mmfStruct *x = hash_entry (a, struct mmfStruct, Element);
  const struct mmfStruct *y = hash_entry (b, struct mmfStruct, Element);

  return x->mapid < y->mapid;
}

void free_mmfiles (struct hash *mmfiles){
  hash_destroy (mmfiles, free_mmfiles_entry);
}

static void free_mmfiles_entry (struct hash_elem *e, void *aux UNUSED) {
  struct mmfStruct *mmf;
  mmf = hash_entry (e, struct mmfStruct, Element);
  mmfiles_free_entry (mmf);
}
