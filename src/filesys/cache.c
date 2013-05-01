/* Code for buffer cache.*/
#include <stdio.h>
#include "lib/string.h"
#include "devices/timer.h"
#include "devices/block.h"
#include "devices/ide.h"
#include "userprog/debug.h"
#include "filesys/cache.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Array for representing buffer cache*/
struct_buf bcache[CACHE_SIZE];

/* Empty cache entry used for initialization*/
struct buf null_cache_entry;

/* Static Variable for clock algorithm*/
static int clock_hand;

/* Lock for access to bcache array*/
struct lock bcache_lock;

/*Lock for read_ahead_sectors list*/
struct lock read_ahead_lock;

/*Read ahead sectors list*/
struct list read_ahead_sectors_list;

/* For periodic flushing of cache*/
int time;

/* Function Declarations*/
void bcache_init(void);
void add_read_ahead_sectors(block_sector_t sector);
void read_cache(block_sector_t sector, void *buffer);
void write_cache(block_sector_t sector,const void *buffer);
void read_cache_bounce(block_sector_t sector,void *buffer, int ofs, int chunk_size);
void write_cache_bounce(block_sector_t sector, void *buffer, int ofs, int chunk_size);
void flush_cache(void);
static int insert_cache_write(block_sector_t sector);
static int insert_cache_read(block_sector_t sector);
static int evict_cache(void);
static void write_cache_to_disk(int index);
static int find_cache_entry(block_sector_t sector);
static int index_for_insertion(void);
static void write_back_func(void *aux UNUSED);
static void read_ahead_thread_func(void *aux UNUSED);
static void read_ahead_sectors();

/* Initialise the buffer cache pre-requisites
 * Initialises the bcache_lock and nulls out the
 * entries in the bcache array.Also sets the time
 * marks the clock entry as -1.Creates a thread for
 * periodic flushing of cache.
 * */
void bcache_init(void)
{
	int index;
	struct list_elem *e;

	lock_init(&bcache_lock);
	lock_init(&read_ahead_lock);
	list_init(&read_ahead_sectors_list);

	clock_hand= MARK_EMPTY;
	null_cache_entry.flag=MARK_EMPTY;
	null_cache_entry.accessed=MARK_EMPTY;
	null_cache_entry.sector=MARK_EMPTY;
	(&null_cache_entry)->data=NULL;
	for(index=0;index<CACHE_SIZE;index++)
		bcache[index]=null_cache_entry;
	thread_create("filesys_cache_write_behind",0, write_back_func,NULL);
	thread_create("filesys_cache_read_ahead",0,read_ahead_thread_func, NULL);
	DPRINTF_CACHE("bcache_init:CACHE INITIALIZED\n");	
}


/* Function called in filesys_cache_write_behind
 * thread for periodic flushing of cache.
 * */
void write_back_func(void *aux UNUSED)
{
	while(true)
	{
		timer_sleep(WRITE_BACK_INTERVAL);
		flush_cache();
	}
}

/* Function for creating a thread for reading
 * blocks to the buffer cache in advance.
 * */
void read_ahead_thread_func(void *aux UNUSED)
{
	while(true)
	{
		read_ahead_sectors();
		thread_yield();
//		printf("spinning\n");
	}
}

void read_ahead_sectors()
{
	struct list_elem *e;
	struct node *temp;
	void *temp_buffer=malloc(BLOCK_SECTOR_SIZE);
	while (!list_empty (&read_ahead_sectors_list))
	{
		lock_acquire(&read_ahead_lock);
		e=list_pop_front(&read_ahead_sectors_list);
		temp=list_entry(e,struct node,list_element);
		read_cache(temp->sector,temp_buffer);
		free(e);
		lock_release(&read_ahead_lock);
	}
}

void add_read_ahead_sectors(block_sector_t sector)
{
	printf("-----------------kamaal ho gaya\n");
	struct node *temp=malloc(sizeof(struct node));
	temp->sector=sector;
	lock_acquire(&read_ahead_lock);
	list_push_back(&read_ahead_sectors_list,&temp->list_element);
	lock_release(&read_ahead_lock);
}


/* Function for reading the entries from buffer cache
 * into the buffer. It takes a sector as input, finds
 * the corresponding entry in buffer cache and copies
 * it to buffer.If entry is not found then it is brought
 * in from the disk by evicting an existing entry from
 * buffer cache.
 * */
void read_cache( block_sector_t sector, void *buffer)
{
	DPRINT_CACHE("read_cache:%x\n",sector);
	int index_to_cache=find_cache_entry(sector);
	if( (index_to_cache >= 0) && (index_to_cache < CACHE_SIZE) )
		memcpy(buffer,(&bcache[index_to_cache])->data, BLOCK_SECTOR_SIZE);
	else 
	{
		index_to_cache=insert_cache_read(sector);
		memcpy(buffer,(&bcache[index_to_cache])->data,BLOCK_SECTOR_SIZE);
	}
}

/* Function for writing the entries from buffer cache
 * into the buffer. It takes a sector as input, finds
 * the corresponding entry in buffer cache and copies
 * data from buffer into the buffer cache entry field.
 * If entry is not found then an entry is evicted from
 * the buffer cache and data from buffer is written to
 * it and marked dirty.
 * */
void write_cache(block_sector_t sector,const void *buffer)
{
	DPRINT_CACHE("write_cache:%x\n",sector);
	int index_to_cache=find_cache_entry(sector);
	if( (index_to_cache >= 0) && (index_to_cache < CACHE_SIZE) )
	{
		bcache[index_to_cache].flag=BUFFER_DIRTY;
		memcpy((&bcache[index_to_cache])->data, buffer, BLOCK_SECTOR_SIZE);
	}
	else 
	{
		index_to_cache=insert_cache_write(sector);
		memcpy((&bcache[index_to_cache])->data, buffer,BLOCK_SECTOR_SIZE);
	}
}


void read_cache_bounce(block_sector_t sector,void *buffer, int ofs, int chunk_size)
{
	uint8_t *temp_buffer=malloc(BLOCK_SECTOR_SIZE);
	read_cache(sector,temp_buffer);
	memcpy(buffer, temp_buffer+ofs, chunk_size);
	free(temp_buffer);
}


void write_cache_bounce(block_sector_t sector, void *buffer, int ofs, int chunk_size)
{
	uint8_t *temp_buffer=malloc(BLOCK_SECTOR_SIZE);
	if((ofs > 0) || (chunk_size < (BLOCK_SECTOR_SIZE - ofs)))
		read_cache(sector,temp_buffer);
	else memset(temp_buffer,0,BLOCK_SECTOR_SIZE);

	memcpy(temp_buffer+ofs, buffer, chunk_size);
	write_cache(sector, temp_buffer);
	free(temp_buffer);
	
}

/* Function for inserting a write entry into the buffer
 * cache array.It acquires the index at which the
 * sector data is to be inserted and then sets the 
 * variables accordingly and returns that index.
 * */
int insert_cache_write(block_sector_t sector)
{
	int index=index_for_insertion();
	lock_acquire(&bcache_lock);
	bcache[index].sector=sector;
	bcache[index].accessed=1;
	bcache[index].flag=BUFFER_DIRTY;
	lock_release(&bcache_lock);
	DPRINT_CACHE("insert_cache_write:INSERTED AT:%d\n",index);
	return index;
}

/* Function for inserting a read entry into the buffer
 * cache array.It acquires the index at which the
 * sector data is to be inserted and then sets the 
 * variables accordingly and reads the data from disk
 * into the buffer cache entry chosen to be inserted
 * and then it returns the index.
 * */
int insert_cache_read(block_sector_t sector)
{
	int index=index_for_insertion();
	lock_acquire(&bcache_lock);
	bcache[index].sector=sector;
	bcache[index].accessed=1;
	bcache[index].flag=BUFFER_VALID;
	block_read(block_get_role(BLOCK_FILESYS),sector,(&bcache[index])->data);
	lock_release(&bcache_lock);
	DPRINT_CACHE("insert_cache_read:INSERTED AT\n",index);
	return index;
}

/* Finds a suitable index for inserting an entry
 * (read/write) into the buffer cache.First it checks
 * if the buffer cache contains any null entry.Returns
 * the null entry if found any.Else it evicts an entry
 * and returns its index for insertion.
 * */
int index_for_insertion(void)
{
	int index=0;
	while(index<CACHE_SIZE)
	{
		if(bcache[index].accessed==MARK_EMPTY)
		{
			(&bcache[index])->data=malloc(BLOCK_SECTOR_SIZE);
			return index;
		}index++;
	}
	index=evict_cache();
	(&bcache[index])->data=malloc(BLOCK_SECTOR_SIZE);
	return index;
}

/* Function for evicting cache. Uses clock algorithm
 * to select which entry to evict.Decrements the accessed
 * field everytime the clock hand passes over the entry.
 * Writes the entry to disk and returns the index of the
 * buffer cache it evicted.
 * */
int evict_cache(void)
{
	int start_point=clock_hand++;
	clock_hand=clock_hand % CACHE_SIZE;
	while(clock_hand!=start_point)
	{
		if(bcache[clock_hand].accessed <= 0)
			break;
		bcache[clock_hand].accessed--;
		clock_hand++;
		clock_hand=clock_hand % CACHE_SIZE;
	}
	DPRINT_CACHE("evict_cache:CLOCK HAND:%d\n",clock_hand);
//	printf("clock hand %d\n",clock_hand);
	write_cache_to_disk(clock_hand);
	return clock_hand;
}

/* Writes the entries marked dirty to the 
 * disk.Marks the entry written to disk as null.
 * */
void write_cache_to_disk(int index)
{
	DPRINT_CACHE("write_cache_to_disk:bcache[index]:%d\n",index);	
	DPRINT_CACHE("write_cache_to_disk:accessed:%d\n",bcache[index].accessed);
	DPRINT_CACHE("write_cache_to_disk:flag:%d\n",bcache[index].flag);
	DPRINT_CACHE("write_cache_to_disk:sector:%d\n",bcache[index].sector);

//	printf("write_cacheto disk %d\n",bcache[index].sector);
	block_write(block_get_role(BLOCK_FILESYS),bcache[index].sector,(&bcache[index])->data);
	lock_acquire(&bcache_lock);
	free((&bcache[index])->data);

	bcache[index]=null_cache_entry;
	lock_release(&bcache_lock);
	DPRINTF_CACHE("write_cache_to_disk:COMPLETE\n");
}

/* Writes the cache entries marked as DIRTY
 * to the disk.
 * */
void flush_cache(void)
{
	int index=0,dirty_entries=0;
	
	while(index<CACHE_SIZE)
	{
		DPRINTF_CACHE("flush_cache: bcache index stats: ");
		DPRINT_CACHE("sector:%d ", bcache[index].sector);
		DPRINT_CACHE("flag:%d ", bcache[index].flag);
		DPRINT_CACHE("accessed:%d\n",bcache[index].accessed);
		
		if( bcache[index].flag==BUFFER_DIRTY )
		{
			write_cache_to_disk(index);
			dirty_entries++;
		}
		index++;
	}
	DPRINT_CACHE("flush_cache:FLUSHED %d dirty_entries\n",dirty_entries);
}

/* Used to find an entry in the buffer cache.
 * Traverses through the array to look for the entry
 * and matches the sector given as the argument. Returns
 * the index if sector matches otherwise it returns -1.
 * */
int find_cache_entry(block_sector_t sector)
{
	DPRINT_CACHE("find_cache_entry:FOR SECTOR:%x\n",sector);
	int index_to_cache=0;
	while(index_to_cache<CACHE_SIZE)
	{
		if(bcache[index_to_cache].sector==sector)
		{
			DPRINT_CACHE("find_cache_entry:INDEX RETURNED:%d\n",index_to_cache);
			bcache[index_to_cache].accessed++;
			return index_to_cache;
		}
		index_to_cache++;
	}
	DPRINTF_CACHE("find_cache_entry:ENTRY NOT FOUND\n");
	return -1;
}
