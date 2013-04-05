#include "threads/thread.h"
#include "lib/kernel/list.h"

void frameInit(void);
void *allocateFrame(enum palloc_flags FLAG, void *page);
void freeFrame(void *frame);
void *evictFrameFor(void *page);
void setFrameAttributes(uint32_t *pte, void *kpage);

struct frameStruct {
	void *frame;
	tid_t tid;
	void *page;
	uint32_t *pageTableEntry;
	struct list_elem listElement;
};

struct list frameTable;
