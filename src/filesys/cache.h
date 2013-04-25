/*Code for buffer cache.*/
/*Structure of cache and working functions*/
#include <stdio.h>
#include <devices/block.h>

#define CACHE_SIZE 	 64
#define FLUSH_TIME 30
#define BUFFER_VALID 0x1
#define BUFFER_DIRTY 0x2
#define BUFFER_BUSY 0x4
#define INSERT_READ 0
#define INSERT_WRITE 1
#define MARK_EMPTY -1
#define TWENTY_SECONDS TIMER_FREQ*20

/* Function Declaration*/
void bcache_init(void);
void read_cache( block_sector_t sector, void *buffer);
void write_cache(block_sector_t sector,const void *buffer);
void read_cache_bounce(block_sector_t sector,void *buffer, int ofs, int chunk_size);
void write_cache_bounce(block_sector_t sector, void *buffer, int ofs, int chunk_size);
void insert_cache_write(block_sector_t sector, const void *buffer);
void insert_cache_read( block_sector_t sector, void *buffer);
int evict_cache(void);
void write_cache_to_disk(int index);
void flush_cache(void);
int find_cache_entry(block_sector_t sector);
int index_for_insertion(void);


typedef struct buf
{
	int flag;
	int accessed;
	block_sector_t sector;
	void *data;
}struct_buf;

