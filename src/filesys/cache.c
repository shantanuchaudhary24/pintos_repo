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

/* Array for representing buffer cache*/
struct_buf bcache[CACHE_SIZE];

/* Empty cache entry used for initialization*/
struct buf null_cache_entry;

/* Static Variable for clock algorithm*/
static int clock_hand;

/* Lock for access to bcache array*/
struct lock bcache_lock;

/* Function Declaration*/
void bcache_init(void);
void read_cache(block_sector_t sector, void *buffer);
void write_cache(block_sector_t sector,const void *buffer);
void insert_cache_write(block_sector_t sector, const void *buffer);
void insert_cache_read(block_sector_t sector, void *buffer);
int evict_cache(void);
void write_cache_to_disk(int index);
void flush_cache(void);
int find_cache_entry(block_sector_t sector);
int index_for_insertion(void);

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
	DPRINTF_CACHE("bcache_init:CACHE INITIALIZED\n");	
}

void read_cache( block_sector_t sector, void *buffer)
{
	DPRINT_CACHE("read_cache:%x\n",sector);
	int index_to_cache=find_cache_entry(sector);
	if( (index_to_cache >= 0) && (index_to_cache < CACHE_SIZE) )
	{
		bcache[index_to_cache].accessed++;
		memcpy(buffer,(&bcache[index_to_cache])->data, BLOCK_SECTOR_SIZE);
	}
	else insert_cache_read(sector,buffer);
}

void write_cache(block_sector_t sector,const void *buffer)
{
	DPRINT_CACHE("write_cache:%x\n",sector);
	int index_to_cache=find_cache_entry(sector);
	if( (index_to_cache >= 0) && (index_to_cache < CACHE_SIZE) )
	{
		bcache[index_to_cache].accessed++;
		bcache[index_to_cache].flag=BUFFER_DIRTY;
		memcpy((&bcache[index_to_cache])->data, buffer, BLOCK_SECTOR_SIZE);
	}
	else insert_cache_write(sector,buffer);
}

void insert_cache_write(block_sector_t sector, const void *buffer)
{
	int index=index_for_insertion();
	//printf("insert_cache_write:acquired lock bcache\n");
	lock_acquire(&bcache_lock);
	bcache[index].sector=sector;
	bcache[index].accessed=1;
	bcache[index].flag=BUFFER_DIRTY;
	memcpy((&bcache[index])->data, buffer,BLOCK_SECTOR_SIZE);
	lock_release(&bcache_lock);
	//printf("insert_cache_write:released lock bcache\n");
	DPRINTF_CACHE("insert_cache_write:COMPLETE\n");
	return;
}


void insert_cache_read(block_sector_t sector, void *buffer)
{
	int index=index_for_insertion();
	//printf("insert_cache_read:acquired lock bcache\n");
	lock_acquire(&bcache_lock);
	bcache[index].sector=sector;
	bcache[index].accessed=1;
	bcache[index].flag=BUFFER_VALID;
	block_read(block_get_role(BLOCK_FILESYS),sector,(&bcache[index])->data);
	memcpy(buffer,(&bcache[index])->data,BLOCK_SECTOR_SIZE);
	lock_release(&bcache_lock);
	//printf("insert_cache_read:released lock bcache\n");
	DPRINTF_CACHE("insert_cache_read:COMPLETE\n");
	return;
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
	write_cache_to_disk(clock_hand);
	return clock_hand;
}

void write_cache_to_disk(int index)
{
	DPRINT_CACHE("write_cache_to_disk:bcache[index]:%d\n",index);	
	DPRINT_CACHE("write_cache_to_disk:accessed:%d\n",bcache[index].accessed);
	DPRINT_CACHE("write_cache_to_disk:flag:%d\n",bcache[index].flag);
	DPRINT_CACHE("write_cache_to_disk:sector:%d\n",bcache[index].sector);

	block_write(block_get_role(BLOCK_FILESYS),bcache[index].sector,(&bcache[index])->data);
	//printf("write_cache_to_disk:acquired lock bcache\n");
	lock_acquire(&bcache_lock);
	free((&bcache[index])->data);
	bcache[index]=null_cache_entry;
	lock_release(&bcache_lock);
	//printf("write_cache_to_disk:released lock bcache\n");
	DPRINTF_CACHE("write_cache_to_disk:COMPLETE\n");
}

void flush_cache(void)
{
	int index=0;
	while(index<CACHE_SIZE)
	{
		if(bcache[index].flag & BUFFER_DIRTY)
			block_write(block_get_role(BLOCK_FILESYS),bcache[index].sector,(&bcache[index])->data);
		index++;
	}
	DPRINTF_CACHE("flush_cache:COMPLETE\n");	
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
			return index_to_cache;
		}
		index_to_cache++;
	}
	DPRINTF_CACHE("find_cache_entry:ENTRY NOT FOUND\n");
	return -1;
}

