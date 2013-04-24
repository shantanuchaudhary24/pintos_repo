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
bool read_cache(struct block *block, block_sector_t sector, void *buffer);
bool write_cache(struct block *block, block_sector_t sector, void *buffer);
bool insert_cache(struct block *block, block_sector_t sector, void *buffer, int insert_type);
void update_time(void);
int evict_cache(void);
void write_cache_to_disk(int index);
void flush_cache(void);
int find_cache_entry(block_sector_t sector);


/* For measuring test performance*/
static int entries_in_cache;
static int disk_access;
static int cache_access;
int64_t time;

/* Initialise the buffer cache pre-requisites
 * Initialises the bcache_lock and nulls out the
 * entries in the bcache array.Also sets the time
 * variable to the current time.
 * */
void bcache_init(void)
{
	DPRINTF_CACHE("bcache_init:CACHE INITIALIZED\n");	
	lock_init(&bcache_lock);
	clock_hand= -1;
	null_cache_entry.flag=MARK_EMPTY;
	null_cache_entry.accessed=MARK_EMPTY;
	null_cache_entry.sector=MARK_EMPTY;
	(&null_cache_entry)->block=NULL;
	(&null_cache_entry)->data=NULL;
	int index;
	for(index=0;index<CACHE_SIZE;index++)
		bcache[index]=null_cache_entry;
	time=timer_ticks();
	entries_in_cache=0;
	disk_access=0;
	cache_access=0;
}

bool read_cache(struct block *block, block_sector_t sector, void *buffer)
{
	cache_access++;
	//update_time();
	bool result=false;
	int index_to_cache=find_cache_entry(sector);
	if( (index_to_cache >= 0) && (index_to_cache < CACHE_SIZE) )
	{
		bcache[index_to_cache].accessed++;
		memcpy(buffer,(&bcache[index_to_cache])->data, BLOCK_SECTOR_SIZE);
		return true;
	}
	else 
	{
		result = insert_cache(block,sector,buffer,INSERT_READ);
		return result;
	}
}

bool write_cache(struct block *block, block_sector_t sector, void *buffer)
{
	cache_access++;
	//update_time();
	bool result=false;
	int index_to_cache=find_cache_entry(sector);
	if( (index_to_cache >= 0) && (index_to_cache < CACHE_SIZE) )
	{
		bcache[index_to_cache].accessed++;
		bcache[index_to_cache].flag=BUFFER_DIRTY;
		memcpy((&bcache[index_to_cache])->data, buffer, BLOCK_SECTOR_SIZE);
		return true;
	}
	else 
	{
		result = insert_cache(block,sector,buffer,INSERT_WRITE);
		return result;
	}
}

bool insert_cache(struct block *block, block_sector_t sector, void *buffer, int insert_type)
{
	int index=0;
	lock_acquire(&bcache_lock);
	bcache[index].sector=sector;
	(&bcache[index])->block=block;
	bcache[index].accessed=1;
	while(index<CACHE_SIZE && bcache[index].accessed==MARK_EMPTY)
	{
		if(insert_type==INSERT_READ)
		{
			bcache[index].flag=BUFFER_VALID;
			block_read(block,sector,(&bcache[index])->data);
			memcpy(buffer,(&bcache[index])->data,BLOCK_SECTOR_SIZE);
			disk_access++;
			entries_in_cache++;
			lock_release(&bcache_lock);
			return true;
		}
		if(insert_type==INSERT_WRITE)
		{
			bcache[index].flag=BUFFER_DIRTY;
			(&bcache[index])->block=block;
			(&bcache[index])->data=malloc(BLOCK_SECTOR_SIZE);
			memcpy((&bcache[index])->data, buffer,BLOCK_SECTOR_SIZE);
			entries_in_cache++;
			lock_release(&bcache_lock);
			return true;
		}
		index++;
	}
	lock_release(&bcache_lock);
	index=evict_cache();	
	lock_acquire(&bcache_lock);
	if(insert_type==INSERT_READ)
	{
		bcache[index].flag=BUFFER_VALID;
		block_read(block,sector,(&bcache[index])->data);
		disk_access++;
		memcpy(buffer,(&bcache[index])->data,BLOCK_SECTOR_SIZE);
		lock_release(&bcache_lock);
		return true;
	
	}
	if(insert_type==INSERT_WRITE)
	{
		bcache[index].flag=BUFFER_DIRTY;
		memcpy((&bcache[index])->data, buffer,BLOCK_SECTOR_SIZE);
		lock_release(&bcache_lock);
		return true;
	}
	lock_release(&bcache_lock);
	return false;
}

int evict_cache(void)
{
	int start_point=clock_hand++;
	while(clock_hand!=start_point)
	{
		if(bcache[clock_hand].accessed <= 0)
			break;
		bcache[clock_hand].accessed--;
		clock_hand++;
		clock_hand=clock_hand % CACHE_SIZE;
	}
	write_cache_to_disk(clock_hand);
	return clock_hand;
}

void write_cache_to_disk(int index)
{
	block_write(block_get_role(BLOCK_FILESYS),bcache[index].sector,(&bcache[index])->data);
	lock_acquire(&bcache_lock);
	free((&bcache[index])->data);
	bcache[index]=null_cache_entry;
	disk_access++;
	entries_in_cache--;
	lock_release(&bcache_lock);
}

void flush_cache(void)
{
	int index=0;
	while(index<CACHE_SIZE)
	{
		if(bcache[index].flag & BUFFER_DIRTY)
			write_cache_to_disk(index);
		index++;
	}
}

int find_cache_entry(block_sector_t sector)
{
	int index_to_cache=0;
	while(index_to_cache<CACHE_SIZE)
	{
		if(bcache[index_to_cache].sector==sector)
			return index_to_cache;
		index_to_cache++;
	}
	return -1;
}

void update_time(void)
{
	if(timer_elapsed(time) >= FLUSH_TIME)
	{
		time=timer_ticks();
		flush_cache();
	}	
}
