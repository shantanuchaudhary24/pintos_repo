#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "lib/kernel/list.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "vm/debug.h"

void frameInit(void);
void *allocateFrame(enum palloc_flags FLAG, void *page);
void freeFrame(void *frame);
void *evictFrameFor(void *page);
void setFrameAttributes(uint32_t *pte, void *kpage);

static bool addFrameToTable(void *frame, void *page);
static void removeFrameFromTable(void *frame);
static struct frameStruct *getFrameFromTable(void *frame);
static struct frameStruct *findFrameForEviction(void);
static bool saveEvictedFrame(struct frameStruct *frame);

static struct lock frameTableLock;

// This pointer is used in clock algorithm
static struct list_elem *eclock;

/*function for intialising the frame Table and frame Table lock*/
void frameInit(void) {
	list_init(&frameTable);
	lock_init(&frameTableLock);
	eclock=list_begin(&frameTable);
}

/* This function allocates a new frame to the calling thread
 * What it does is, first of all it calls palloc_get_page and 
 * tries to get a new frame if any free frame is present. In that
 * case, it maps this frame to the asked page and the thread and stores
 * it in the frame Table. In case no free frame is available, it 
 * tries to evict a frame out of the frame Table based on the clock
 * algorithm. If eviction returns a frame which is not null, this function
 * marks the corresponding changes in the frame table and then returns 
 * this frame to the calling function.
 * */
void *allocateFrame(enum palloc_flags FLAG, void *page) {
	
	void * new_frame = NULL;
	
	if((FLAG & PAL_USER)) {
		DPRINTF_FRAME("allocateFrame: ALLOC NEW FRAME\n");

		if((FLAG & PAL_ZERO))
			new_frame = palloc_get_page(PAL_USER|PAL_ZERO);
		else
			new_frame = palloc_get_page(PAL_USER);
	}
	
	if((FLAG & PAL_USER) && new_frame == NULL) {
		
		DPRINTF_FRAME("allocateFrame: EVICTING\n");
		new_frame = evictFrameFor(page);
		
		if(new_frame == NULL)
			PANIC("Can't evict Frame");
	}
	else if(page!=0) {
		DPRINTF_FRAME("allocateFrame:ADD FRAME TO TABLE\n");
		
		if(!addFrameToTable(new_frame, page))
			PANIC("Can't add Frame Struct to the Frame Table");
	}
	
	return new_frame;
}

/* This function frees a given frame */
void freeFrame(void *frame){
	removeFrameFromTable(frame);
	palloc_free_page(frame);
}

/* This function evicts a frame for a given page of a given thread. What
 * it does is that, it first find a frame for eviction and then saves it
 * (in file / swap) and then changes the details in the frame to the new
 * page and current thread. It finally returns the frame of the evicted
 * frame Table entry.
 * */
void *evictFrameFor(void *page) {
	struct frameStruct *evictedFrame;

	lock_acquire(&frameTableLock);
	DPRINTF_FRAME("evictFrameFor: successfuly acquired lock for eviction\n");

	evictedFrame = findFrameForEviction();

	if(evictedFrame == NULL)
		PANIC("Can't find any Frame to evict");

	if(!saveEvictedFrame(evictedFrame))
		PANIC("Can't save Evicted Frame");

	evictedFrame->tid = thread_current()->tid;
	evictedFrame->page = page;

	lock_release(&frameTableLock);
	DPRINTF_FRAME("evictFrameFor: successfuly released lock for eviction\n");
	
	return evictedFrame->frame;
}

/* This function finds a already existing frame for eviction based on clock
 * algorithm. First of all it starts traversing from the entry pointed to
 * by the eclock pointer declared above. This stores the location of the
 * last evicted entry. It looks for a frame entry whose page is marked
 * not accessed. If it reaches the tail it starts from the beginning. If
 * in the current frame, page is marked accessed, it marks it not accessed.
 * So, in two traversel, it will definitely find 1 frame marked not accessed.
 * it returns this frame table entry.
 * */
static struct frameStruct *findFrameForEviction(void){
	struct frameStruct *frame = NULL;
	struct thread *t;
	bool found = false;
	bool accessed;
	DPRINTF_FRAME("findFrameForEviction: enter\n");	
	for(; !found ; eclock = list_next (eclock)){
		if(eclock == list_end(&frameTable))
			eclock = list_begin(&frameTable);

		frame = list_entry (eclock, struct frameStruct, listElement);
		ASSERT(frame != NULL);
		t = get_thread_from_tid(frame->tid);
		ASSERT(t != NULL);
		accessed = pagedir_is_accessed(t->pagedir, frame->page);
		if(!accessed) {
			found = true;
		}
		else {
			pagedir_set_accessed(t->pagedir, frame->page, false);
		}
	}
	DPRINTF_FRAME("findFrameForEviction: RETURN SUCCESS\n");
	return frame;
}

/* This function saves the frame passed to it. What it does is that, first
 * of all it finds for a entry in the supplementary page table for the
 * page mapped to this frame. If it doesn't find one, it adds one of type
 * SWAP. Now if the page in pagedir is set accessed and the page type is
 * MMF, then we write it in file. else if it is accessed (i.e. its type is
 * not MMF) or its type is not file, then we write it in swap slot. all the
 * deatils of the swap slot are filled in the supplementary page table.
 * if all this is successful then it returns true, else false.
 * */
static bool saveEvictedFrame(struct frameStruct *frame) {
	
	DPRINTF_FRAME("saveEvictedFrame: enter\n");
	
	struct thread *t = get_thread_from_tid(frame->tid);
	ASSERT(t!=NULL);
	struct supptable_page *spte = get_supptable_page(&t->suppl_page_table, frame->page,t);
	size_t swapSlotID = 0;

	if(spte == NULL) {
		
		DPRINTF_FRAME("saveEvictedFrame: spte is null\n");
		
		spte = calloc(1, sizeof(*spte));
		spte->uvaddr = frame->page;
		spte->page_type = SWAP;
		
		if(!supptable_add_page(&t->suppl_page_table, spte, t))
			return false;
		
		DPRINTF_FRAME("saveEvictedFrame: spte added to supp_table\n");
	}

	if(pagedir_is_dirty(t->pagedir, spte->uvaddr) && (spte->page_type & MMF))
		write_page_to_file(spte);
	else if(pagedir_is_dirty (t->pagedir, spte->uvaddr) || !(spte->page_type & FILE)) {
		
		swapSlotID = swap_out_page(spte->uvaddr);
		
		if(swapSlotID == SWAP_ERROR)
			return false;
		
		spte->page_type = spte->page_type | SWAP;
		
		DPRINTF_FRAME("saveEvictedFrame: swapped out\n");
	}
	
	DPRINTF_FRAME("saveEvictedFrame: frame saved\n");
	
	memset(frame->frame, 0, PGSIZE);
	spte->swap_slot_index = swapSlotID;
	spte->swap_writable =*frame->pageTableEntry & PTE_W;
	spte->is_page_loaded = false;
	pagedir_clear_page(t->pagedir, spte->uvaddr);

	DPRINTF_FRAME("saveEvictedFrame: out\n");
	
	return true;
}

/* This function adds the newly allocated frame to the frame Table. It
 * mallocs one size of (frameStruct) memory sets all the details and then
 * adds it into the frame Table. It adds it just before the eclock pointer.
 * because eclock is the location of last eviction and we start searching for
 * eviction from there. so, this location acts as the begin of the list, so
 * we just add the frame entry to the end of the list.
 * */
static bool addFrameToTable(void *frame, void *page) {
	
	struct frameStruct *newFrameEntry;
	newFrameEntry = getFrameFromTable(frame);
	
	DPRINT_FRAME("search page addr:%x\n",page);
	
	if(newFrameEntry!=NULL) {
		DPRINTF_FRAME("addFrameToTable: REMOVED PREV FRAME\n");
		removeFrameFromTable(frame);
	}
	
	newFrameEntry = calloc (1, sizeof(*newFrameEntry));
	
	if(newFrameEntry == NULL) {
		DPRINTF_FRAME("addFrameToTable: FAIL\n");
		return false;
	}
	newFrameEntry->frame = frame;
	newFrameEntry->tid = thread_current()->tid;
	newFrameEntry->page = page;
	
	lock_acquire(&frameTableLock);
	list_insert(eclock,&newFrameEntry->listElement);
	lock_release(&frameTableLock);

	DPRINTF_FRAME("addFrameToTable: SUCCESS\n");
	return true;
}

/* Function to remove the specified frame form the frame Table.
 * Searches for the given frame and removes it from the list.
 * */
static void removeFrameFromTable(void *frame) {
	
	struct list_elem *temp;
	struct frameStruct *frameTableEntry;

	lock_acquire(&frameTableLock);
	
	
	for (temp = list_begin (&frameTable); temp != list_end (&frameTable); temp = list_next (temp)){
	
		frameTableEntry = list_entry(temp, struct frameStruct, listElement);
	
		if(frameTableEntry->frame == frame){
			DPRINTF_FRAME("removeFromTable: about to remove\n");
			list_remove(temp);
			free(frameTableEntry);
			break;
		}
	}

	lock_release(&frameTableLock);
}

/* Removes all entries of a given thread. Used while a thread exits
 * */
void removeFrameEntriesFor(tid_t t){
	struct list_elem *temp;
	struct frameStruct *fte;

	lock_acquire(&frameTableLock);
	
	for(temp = list_begin(&frameTable); temp!=list_end(&frameTable); temp = list_next(temp)){
	
		fte = list_entry(temp, struct frameStruct, listElement);
	
		if(fte!=NULL && fte->tid == t){
			list_remove(temp);
			free(fte);
		}
	}
	
	lock_release(&frameTableLock);
}

/*Searches for a given frame in the frame Table and returns it*/
static struct frameStruct *getFrameFromTable(void *frame) {
	struct list_elem *temp;
	struct frameStruct *frameTableEntry;

	lock_acquire(&frameTableLock);
	
	temp = list_head(&frameTable);

	while((temp = list_next(temp)) != list_tail(&frameTable)) {

		frameTableEntry = list_entry(temp, struct frameStruct, listElement);

		if(frameTableEntry->frame == frame)
			break;

		frameTableEntry = NULL;
	}

	lock_release(&frameTableLock);

	return frameTableEntry;
}

/*sets the pte variable inside the frame struct. This variable is later
 * required to set the swap_writable bit of supplementary page table
 * */
void setFrameAttributes(uint32_t *pte, void *kpage) {
	struct frameStruct *temp;
	temp = getFrameFromTable(kpage);
	
	if(temp!=NULL) {
		temp->pageTableEntry=pte;
	}
}
