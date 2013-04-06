#include "threads/thread.h"
#include "lib/kernel/list.h"

void frameInit(void);
void *allocateFrame(enum palloc_flags flag, void *page);
void freeFrame(void *frame);
void *evictFrameFor(void *page);
void setFrameAttributes(uint32_t *pte, void *kpage);
void removeFrameEntriesFor(tid_t t);

/*structure fro storing frame mappings in the frame Table*/
struct frameStruct {
	void *frame;
	tid_t tid;
	void *page;
	uint32_t *pageTableEntry;
	struct list_elem listElement;
};

/*Global frame Table used to store all the mappings of the frames with 
 * the pages of any thread*/
struct list frameTable;
