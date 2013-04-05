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
static struct lock lockForEviction;

void frameInit(void)
{
	list_init(&frameTable);
	lock_init(&frameTableLock);
	lock_init(&lockForEviction);
}

void *allocateFrame(enum palloc_flags FLAG, void *page)
{
	void * new_frame = NULL;
	// allocate a new page from user pool
	if((FLAG & PAL_USER))
	{
		DPRINTF_FRAME("allocateFrame:ALLOC NEW FRAME\n");

		if((FLAG & PAL_ZERO))
			new_frame = palloc_get_page(PAL_USER|PAL_ZERO);
		else
			new_frame = palloc_get_page(PAL_USER);
	}
	
	// if can't allocate a new page from user pool, try evicting one from the frameTable
	if(new_frame == NULL)
	{
		DPRINTF_FRAME("allocateFrame:EVICTING\n");
		new_frame = evictFrameFor(page);
		// if evicting fails, PANIC the kernel
		if(new_frame == NULL)
		{
			PANIC("Can't evict Frame");
		}
		// else just exit and return the frame (this frame is already in the frameTable)
	}
	else if(page!=0)
	{
		// if allocation of a new page succeeded, add this new frame to the frameTable
		DPRINTF_FRAME("allocateFrame:ADD FRAME TO TABLE\n");
		if(!addFrameToTable(new_frame, page))
			PANIC("Can't add Frame Struct to the Frame Table");
	}
	
	// return the frame;
	return new_frame;
}

void freeFrame(void *frame){
	removeFrameFromTable(frame);
	palloc_free_page(frame);
}

void *evictFrameFor(void *page)
{
	struct frameStruct *evictedFrame;

	lock_acquire(&lockForEviction);

	evictedFrame = findFrameForEviction();

	if(evictedFrame == NULL)
		PANIC("Can't find any Frame to evict");

	if(!saveEvictedFrame(evictedFrame))
		PANIC("Can't save Evicted Frame");

	evictedFrame->tid = thread_current()->tid;
	evictedFrame->page = page;

	lock_release(&lockForEviction);

	return evictedFrame->frame;
}

static struct frameStruct *findFrameForEviction(void){
	struct frameStruct *frame = NULL;
	struct frameStruct *temp;
	struct list_elem *e;
	struct thread *t;
	
	int i = 0;
	
	for(i = 0; i < 2; i++){
	
		e = list_head(&frameTable);
		
		while( (e = list_next(e)) != list_tail(&frameTable))
		{
			temp = list_entry (e, struct frameStruct, listElement);
			t = get_thread_from_tid(temp->tid);
			//printf("page in temp:%x frame in temp:%x\n",temp->page,temp->frame);
			if(!pagedir_is_accessed(t->pagedir, temp->page))
			{
				frame = temp;
				list_remove(e);
				list_push_back(&frameTable, e);
				break;
			}
			else
			{
				pagedir_set_accessed(t->pagedir, temp->page, false);
			}

		}
		
		if(frame != NULL)
			break;
	}
	DPRINTF_FRAME("findFrameForEviction:RETURN SUCCESS\n");
	return frame;
}

static bool saveEvictedFrame(struct frameStruct *frame)
{
	struct thread *t = get_thread_from_tid(frame->tid);
	struct supptable_page *spte = get_supptable_page(&t->suppl_page_table, frame->page);
	size_t swapSlotID = 0;

	if(spte == NULL)
	{
		spte = calloc(1, sizeof(spte));
		spte->uvaddr = frame->page;
		spte->page_type = SWAP;
//		printf("NULL return in loop1\n");
		if(!supptable_add_page(&t->suppl_page_table, spte))
			return false;
	}

	if(pagedir_is_dirty(t->pagedir, spte->uvaddr) && (spte->page_type & MMF))
		write_page_to_file(spte);
	else if(pagedir_is_dirty (t->pagedir, spte->uvaddr) || !(spte->page_type & FILE))
	{
		swapSlotID = swap_out_page(spte->uvaddr);

		if(swapSlotID == SWAP_ERROR)
			return false;
		spte->page_type = spte->page_type | SWAP;
	}

	memset(frame->frame, 0, PGSIZE);
	spte->swap_slot_index = swapSlotID;
	spte->swap_writable =*frame->pageTableEntry & PTE_W;
	spte->is_page_loaded = false;
	pagedir_clear_page(t->pagedir, spte->uvaddr);

	return true;
}

// function to add the newly allocated frame to the frame Table
static bool addFrameToTable(void *frame, void *page)
{
	struct frameStruct *newFrameEntry;
	newFrameEntry = getFrameFromTable(frame);
	//printf("search page addr:%x\n",page);
	if(newFrameEntry!=NULL)
	{
		DPRINTF_FRAME("addFrameToTable:REMOVE PREV FRAME");
		removeFrameFromTable(newFrameEntry);
	}
	newFrameEntry = calloc (1, sizeof(*newFrameEntry));
	if(newFrameEntry == NULL)
	{
		DPRINTF_FRAME("addFrameToTable:FAIL");
		return false;
	}
	newFrameEntry->frame = frame;
	newFrameEntry->tid = thread_current()->tid;
	newFrameEntry->page = page;
	
	lock_acquire(&frameTableLock);
	list_push_back(&frameTable, &newFrameEntry->listElement);
	lock_release(&frameTableLock);

	DPRINTF_FRAME("addFrameToTable:SUCCESS\n");
	return true;
}

//function to remove the specified frame form the frame Table

static void removeFrameFromTable(void *frame)
{
	struct list_elem *temp;
	struct frameStruct *frameTableEntry;

	lock_acquire(&frameTableLock);
	
	temp = list_head(&frameTable);
	
	while((temp = list_next(temp)) != list_tail(&frameTable)){
		frameTableEntry = list_entry(temp, struct frameStruct, listElement);
		if(frameTableEntry->frame == frame){
			list_remove(temp);
			free(frameTableEntry);
			break;
		}
	}

	lock_release(&frameTableLock);
}


static struct frameStruct *getFrameFromTable(void *frame)
{
	struct list_elem *temp;
	struct frameStruct *frameTableEntry;
	lock_acquire(&frameTableLock);
	
	temp = list_head(&frameTable);
	
	while((temp = list_next(temp)) != list_tail(&frameTable))
	{
		frameTableEntry = list_entry(temp, struct frameStruct, listElement);
		if(frameTableEntry->frame == frame)
			break;
		frameTableEntry = NULL;
	}
	lock_release(&frameTableLock);
	return frameTableEntry;
}

void setFrameAttributes(uint32_t *pte, void *kpage)
{
	struct frameStruct *temp;
	temp=getFrameFromTable(kpage);
	if(temp!=NULL)
	{
		temp->pageTableEntry=pte;
	}
}
