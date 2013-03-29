

struct frameStruct {
	void *frame;
	tid_t tid;
	void *page;
	struct list_elem listElement;
}

struct list frameTable;


void frameInit();
void *allocateFrame(enum palloc_flags FLAG, void *page);
void freeFrame(void *frame);
void *evictFrameFor(void *page);
