#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "vm/debug.h"

static struct swap_struct *swap_space;

static struct lock swap_lock;

void init_swap_space(void);
size_t swap_out_page(void *vaddr);
void swap_in_page(size_t swap_slot,void *vaddr);
void swap_clear_slot(size_t swap_slot);
void destroy_swap_space(void);
size_t bitmap_table_size(void);

/* Initialize the swap space which consists
 * of the swap disk and swap table(bitmap structure).
 * If swap disk assignment fails, it returns NULL.
 * If swap disk is assigned then it creates a bitmap table
 * for representing swap table.The bit count is decided by
 * determining number of pages the swap device can contain(refer swap.h).
 * At the end initializes the swap table bits to false.
 * */
void init_swap_space(void)
{
	lock_init(&swap_lock);
    swap_space=(struct swap_struct *)malloc(sizeof (struct swap_struct));
	swap_space->swap_disk=block_get_role(BLOCK_SWAP);

    if(swap_space->swap_disk==NULL)
        PANIC("NO SWAP DISK FOUND");
    
    size_t bit_cnt=bitmap_table_size();
    swap_space->swap_table=bitmap_create(bit_cnt);

	if(swap_space->swap_table==NULL)
        PANIC("SWAP TABLE COULD NOT BE CREATED");

	bitmap_set_all(swap_space->swap_table,false);
}

/* Destroy the swap space by destroying the swap table.
 * */
void destroy_swap_space(void)
{
	bitmap_destroy(swap_space->swap_table);
	free(swap_space);
}

/* Finds an empty slot in the swap table and write the page
 * represented by vaddr into the empty swap slot of the swap
 * disk.It returns SWAP_ERROR if writing to the
 * swap disk fails. On success it returns the swap table slot.
 * */
size_t swap_out_page(void *vaddr)
{
    void* buffer;
    block_sector_t sector;
    DPRINTF_SWAP("swap_out_page:BEGIN\n");
    lock_acquire(&swap_lock);
    size_t swap_slot=bitmap_scan(swap_space->swap_table,SWAP_TABLE_START,SWAP_TABLE_CNT,false);
    
    if (swap_slot==BITMAP_ERROR)
    {
    	lock_release(&swap_lock);
    	PANIC("swap_out_page:SWAP SLOT ERROR\n");
    	return SWAP_ERROR;
    }
    
    size_t counter=0;
    for(counter=0;counter<NUM_SECTORS_PER_PAGE;counter++)
    {
        sector=swap_slot*NUM_SECTORS_PER_PAGE+counter;
        buffer=vaddr+counter*BLOCK_SECTOR_SIZE;
        block_write(swap_space->swap_disk,sector,buffer);
    }
    bitmap_mark(swap_space->swap_table,swap_slot);
    lock_release(&swap_lock);
    return swap_slot;
}

/* Swaps a page of data in a slot represented by swap_slot to
 * a page pointed by vaddr by reading(block_read) from the swap_disk.
 * Resets the corresponding swap slot bit in the swap table bitmap.
 * */
void swap_in_page(size_t swap_slot,void *vaddr)
{
	void* buffer;
	block_sector_t sector;
	size_t counter=0;
	DPRINTF_SWAP("swap_in_page:BEGIN\n");
	lock_acquire(&swap_lock);
	for(counter=0;counter<NUM_SECTORS_PER_PAGE;counter++)
	{
		sector=swap_slot*NUM_SECTORS_PER_PAGE+counter;
		buffer=vaddr+counter*BLOCK_SECTOR_SIZE;
		block_read(swap_space->swap_disk,sector,buffer);
	}
	bitmap_reset(swap_space->swap_table,swap_slot);
	lock_release(&swap_lock);
}

/* Resets the bit pointed by swap_slot.This is needed
 * when a page is freed from the supplementary table.
 * */
void swap_clear_slot(size_t swap_slot)
{
	DPRINTF_SWAP("swap_clear_slot:BEGIN\n");
	lock_acquire(&swap_lock);
	if(bitmap_test(swap_space->swap_table,swap_slot)==true)
	{
		DPRINTF_SWAP("swap_clear_slot:SWAP SLOT CLEARED\n");
		bitmap_reset(swap_space->swap_table,swap_slot);
	}
	else {
		DPRINTF_SWAP("swap_clear_slot:SWAP SLOT ALREADY CLEAR\n");
	}
	lock_release(&swap_lock);
}

/* Function for determining the swap table size
 * */
size_t bitmap_table_size(void)
{
	size_t bit_cnt=0;
	int disk_size=block_size(swap_space->swap_disk);
	int sectors_per_page=NUM_SECTORS_PER_PAGE;
	DPRINT_SWAP("init_swap_space:BLOCK_SIZE:%d\n",disk_size);
	DPRINT_SWAP("init_swap_space:NUM_SECTORS_PER_PAGE:%d\n",sectors_per_page);
	bit_cnt=disk_size/sectors_per_page;
	DPRINT_SWAP("init_swap_space:BIT_CNT:%d\n",bit_cnt);
	return bit_cnt;
}
