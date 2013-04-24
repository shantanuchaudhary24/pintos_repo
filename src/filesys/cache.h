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

/* Function Declaration*/
void bcache_init(void);
bool read_cache(struct block *block, block_sector_t sector, void *buffer);
bool write_cache(struct block *block, block_sector_t sector, void *buffer);
bool insert_cache(struct block *block, block_sector_t sector, void *buffer, int insert_type);
void update_time(void);
int evict_cache(void);
void write_cache_to_disk(int index);
void flush_cache(void);
int find_cache_entry(block_sector_t sector);


typedef struct buf
{
	int flag;
	int accessed;
	block_sector_t sector;
	struct block *block;
	void *data;
}struct_buf;

