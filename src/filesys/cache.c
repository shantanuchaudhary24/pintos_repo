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

/* For periodic flushing of cache*/
int time;

/* Function Declaration*/
void bcache_init(void);
void read_ahead_thread(block_sector_t sector);
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
static void read_ahead_func(void *aux);


/* Initialise the buffer cache pre-requisites
 * Initialises the bcache_lock and nulls out the
 * entries in the bcache array.Also sets the time
 * variable to the current time.
 * */
void bcache_init(void)
{
	int index;
	lock_init(&bcache_lock);
	clock_hand= MARK_EMPTY;
	null_cache_entry.flag=MARK_EMPTY;
	null_cache_entry.accessed=MARK_EMPTY;
	null_cache_entry.sector=MARK_EMPTY;
	(&null_cache_entry)->data=NULL;
	for(index=0;index<CACHE_SIZE;index++)
		bcache[index]=null_cache_entry;
	thread_create("filesys_cache_write_behind",0, write_back_func,NULL);
	DPRINTF_CACHE("bcache_init:CACHE INITIALIZED\n");	
}

void write_back_func(void *aux UNUSED)
{
	while(true)
	{
		timer_sleep(WRITE_BACK_INTERVAL);
		flush_cache();
	}
}

void read_ahead_thread(block_sector_t sector)
{
	block_sector_t *temp = malloc(sizeof(block_sector_t));
	if(temp!=NULL)
	{
		*temp=sector+1;
		thread_create("filesys_cache_read_ahead",0,read_ahead_func, temp);
	}
}

void read_ahead_func(void *aux)
{
	block_sector_t sector= *(block_sector_t *)aux;
	insert_cache_read(sector);
	free(aux);
}

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
	printf("clock hand %d\n",clock_hand);
	write_cache_to_disk(clock_hand);
	return clock_hand;
}

void write_cache_to_disk(int index)
{
	DPRINT_CACHE("write_cache_to_disk:bcache[index]:%d\n",index);	
	DPRINT_CACHE("write_cache_to_disk:accessed:%d\n",bcache[index].accessed);
	DPRINT_CACHE("write_cache_to_disk:flag:%d\n",bcache[index].flag);
	DPRINT_CACHE("write_cache_to_disk:sector:%d\n",bcache[index].sector);

	printf("write_cacheto disk %d\n",bcache[index].sector);
	block_write(block_get_role(BLOCK_FILESYS),bcache[index].sector,(&bcache[index])->data);
	lock_acquire(&bcache_lock);
	free((&bcache[index])->data);

	bcache[index]=null_cache_entry;
	lock_release(&bcache_lock);
	DPRINTF_CACHE("write_cache_to_disk:COMPLETE\n");
}

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
