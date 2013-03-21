

struct frameStruct {
	void *frame;
	uint32_t *pagedir;
	void *page;
	struct list_elem listElement;
}

struct list frameTable;


void frameInit();
void *allocateFrame(enum palloc_flags FLAG, void *page);
void *evictFrameFor(void *page);
