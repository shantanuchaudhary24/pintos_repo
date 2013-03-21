#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "vm/frame.h"


static bool addFrameToList(void *frame, void *page);
static void removeFrameFromTable(void *frame);
static struct frameStruct *getFrameFromTable(void *frame);

static struct lock frameTableLock;

void frameInit(){
	list_init(&frameTable);
	lock_init(&frameTableLock);
}

void *allocateFrame(enum palloc_flags FLAG, void *page){
	void * new_frame;
	
	// allocate a new page from user pool
	if((FLAG & PAL_USER)){
		if((FLAG & PAL_ZERO)){
			new_frame = palloc_get_page(PAL_USER|PAL_ZERO);
		}
		else{
			new_frame = palloc_get_page(PAL_USER);
		}
	}
	
	// if can't allocate a new page from user pool, try evicting one from the frameTable
	if(new_frame == NULL){
		// depending upon the implementation of evictFrame() we will
		// need to update the frameStruct->page of the Frame Table Entry
		// corresponding to the new_frame
		new_frame = evictFrameFor(page);
		// if evicting fails, PANIC the kernel
		if(new_frame == NULL){
			PANIC("Can't evict Frame");
		}
		// else just exit and return the frame (this frame is already in the frameTable)
	}
	else {
		// if allocation of a new page succeeded, add this new frame to the frameTable
		if(!addFrameToTable(new_frame, page))
			PANIC("Can't add Frame Struct to the List");
	}
	
	// return the frame;
	return new_frame;
}


void *evictFrameFor(void *page){
	//find a frame to evict
	
	//save its contents in the swap space
	
	//return the frame with updated frameStruct->page = page;
	return NULL;
}


// function to add the newly allocated frame to the frame Table
static bool addFrameToTable(void *frame, void *page){
	struct frameStruct *newFrameEntry;
	
	newFrameEntry = calloc (1, sizeof(*newFrameEntry));
	
	if(newFrameEntry == NULL)
		return false;
	
	newFrameEntry->frame = frame;
	newFrameEntry->pagedir = thread_current()->pagedir;
	newFrameEntry->page = page;
	
	lock_acquire(&frameTableLock);
	list_push_back(&frameTable, &newFrameEntry->listElement);
	lock_release(&frameTableLock);
}

//function to remove the specified frame form the frame Table

static void removeFrameFromTable(void *frame){
	struct list_elem *temp;
	struct frameStruct *frameTableEntry;
	
	lock_acquire(&frameTableLock);
	
	temp = list_head(&frameTable);
	
	while(temp != list_tail(&frameTable)){
		frameTableEntry = list_entry(temp, struct frameStruct, listElement);
		if(frameTableEntry->frame == frame){
			list_remove(temp);
			free(frameTableEntry);
			break;
		}
		temp = list_next(temp);
	}
	
	lock_release(&frameTableLock);
}


static struct frameStruct *getFrameFromTable(void *frame){
	struct list_elem *temp;
	struct frameStruct *frameTableEntry;
	
	lock_acquire(&frameTableLock);
	
	temp = list_head(&frameTable);
	
	while(temp != list_tail(&frameTable)){
		frameTableEntry = list_entry(temp, struct frameStruct, listElement);
		if(frameTableEntry->frame == frame)
			break;
		frameTableEntry = NULL;
		temp = list_next(temp);
	}
	
	lock_release(&frameTableLock);
	
	return frameTableEntry;
}
