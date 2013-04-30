/*Code for buffer cache.*/
/*Structure of cache and working functions*/
#include <stdio.h>
#include <devices/block.h>

#define CACHE_SIZE 	 64
#define BUFFER_VALID 1
#define BUFFER_DIRTY 2
#define MARK_EMPTY -1
#define WRITE_BACK_INTERVAL TIMER_FREQ*20

/* Function Declaration*/
void bcache_init(void);
void read_ahead_thread(block_sector_t sector);
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

