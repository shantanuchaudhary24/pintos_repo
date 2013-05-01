/*Code for buffer cache.*/
/*Structure of cache and working functions*/
#include <stdio.h>
#include <devices/block.h>
#include <lib/kernel/list.h>

#define CACHE_SIZE 	 64
#define BUFFER_VALID 1
#define BUFFER_DIRTY 2
#define MARK_EMPTY -1
#define WRITE_BACK_INTERVAL TIMER_FREQ*20

/* Function Declaration*/
void bcache_init(void);
void add_read_ahead_sectors(block_sector_t sector);
void read_cache(block_sector_t sector, void *buffer);
void write_cache(block_sector_t sector,const void *buffer);
void read_cache_bounce(block_sector_t sector,void *buffer, int ofs, int chunk_size);
void write_cache_bounce(block_sector_t sector, void *buffer, int ofs, int chunk_size);
void flush_cache(void);

typedef struct buf
{
	int flag;
	int accessed;
	block_sector_t sector;
	void *data;
}struct_buf;

struct node
{
	struct list_elem list_element;
	block_sector_t sector;
};
