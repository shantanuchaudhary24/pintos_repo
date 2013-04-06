#include "userprog/mmf.h"
#include "threads/thread.h"
//#include "lib/user/syscall.h"
#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "vm/debug.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"

extern struct lock filesys_lock;

static void mmfiles_free_entry (struct mmfStruct* mmfile);
static unsigned mmfile_hash (const struct hash_elem *a, void *aux UNUSED);
static bool mmfile_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
static unsigned mmfile_hash (const struct hash_elem *a, void *aux UNUSED);
static void free_mmfiles_entry (struct hash_elem *e,void *aux UNUSED);

/*function for initialising the mmfiles hash table*/
bool mmfiles_init(struct hash *mmfiles){
	return hash_init (mmfiles, mmfile_hash, mmfile_less, NULL);
}

/*function for insertnig a memory mapping into the mmfiles hash table
 * it starts from addr and maps all entries in multiples of pgsize into
 * the supplementary page table it stores the starting addr, pointer to
 * the file opened and the page count of the mapped file in the mmfiles
 * struct*/
mapid_t mmfiles_insert (void *addr, struct file* file, int32_t len){
	
	struct thread *t = thread_current ();
	struct mmfStruct *mmf;
	struct hash_elem *result;
	int i = 0;
	
	DPRINTF_MMF("mmfile_insert: enter\n");
	
	if ((mmf = calloc (1, sizeof *mmf)) == NULL)
		return -1;

	mmf->mapid = t->mapid_allocator++;
	mmf->file = file;
	mmf->begin_addr = addr;
	
	for(;len > 0; len-=PGSIZE){
		if(!supptable_add_file(t, MMF, file, ((i)*PGSIZE), 
			(addr + (i)*PGSIZE), (len < PGSIZE ? len : PGSIZE), 0, NULL))
			return -1;
		i++;
	}
	
	DPRINTF_MMF("mmfile_insert: added to supptable\n");
	
	mmf->pageCount = i;
	result = hash_insert (&t->mmfiles, &mmf->Element);
	
	DPRINTF_MMF("mmfile_insert: added to hash table: end\n");
	
	return (result!=NULL) ? -1 : mmf->mapid;
}

/*function for removing the mapping of given id. it removes the entry from
 * the hash table using index on mapid and then it calls mmfiles_free_entry
 * to free all the mappings from the supplementary page table.*/
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
	DPRINTF_MMF("mmfiles_remove: removed from hash table: end\n");
}

/*This function removes all the entries of a given mapping from the 
 * supplementary page table. It starts from the begin_addr stored in the 
 * mapping struct and removes pageCount (stored in struct) entries from 
 * the supplementary page table. But before removing, it checks whether the
 * that page table entry is dirty or not, if it is dirty then, it saves it
 * in the file (whose pointer is saved in the struct). When all the entries
 * are removes, it closes the file correspoinding to that mapping.*/
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
				lock_acquire(&filesys_lock);
				file_seek(spte_ptr->file, spte_ptr->offset);
				file_write(spte_ptr->file, spte_ptr->uvaddr, spte_ptr->read_bytes);
				lock_release(&filesys_lock);
			}
			free (spte_ptr);
		}
	}
	
	DPRINTF_MMF("mmfiles_free_entry: written to files, removed from supptable\n");
	
	lock_acquire (&filesys_lock);
	file_close (mmfile->file);
	lock_release (&filesys_lock);
	
	DPRINTF_MMF("mmfiles_free_entry: closed file : end\n");

	free (mmfile);
}

/*used for hash. passed as parameter during intialisation*/
static unsigned mmfile_hash (const struct hash_elem *a, void *aux UNUSED) {
	const struct mmfStruct *b = hash_entry (a, struct mmfStruct, Element);
	
	return hash_bytes (&b->mapid, sizeof(b->mapid));
}

/*used for hash. passed as parameter during intialisation*/
static bool mmfile_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	const struct mmfStruct *x = hash_entry (a, struct mmfStruct, Element);
	const struct mmfStruct *y = hash_entry (b, struct mmfStruct, Element);

	return x->mapid < y->mapid;
}

/*used for destroying the complete mmfiles hash table*/
void free_mmfiles (struct hash *mmfiles){
	hash_destroy(mmfiles,free_mmfiles_entry);
}

/*used for destroying a single hash elem from the hash table. passed as
 * argument destructor to hash_destroy. will be used by it.*/
static void free_mmfiles_entry (struct hash_elem *e,void *aux UNUSED) {
	struct mmfStruct *mmf;
	mmf = hash_entry (e, struct mmfStruct, Element);
	mmfiles_free_entry (mmf);
}
