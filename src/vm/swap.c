#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/swap.h"

/* Swap disk */
struct block *swap_disk;

/* Swap table(Bitmap Table) */
static struct bitmap *swap_table;

void init_swap_space(void);
size_t swap_out_page(void *vaddr);
void swap_in_page(size_t swap_slot,void *vaddr);
void swap_clear_slot(size_t swap_slot);

/* Initialize the swap space which consists
 * of the swap disk and swap table(bitmap structure).
 * If swap disk assignment fails, it returns NULL.
 * f swap disk is assigned then it creates a bitmap table
 * for representing swap table.The bit count is decided by
 * determining number of pages the swap device can contain(refer swap.h).
 * At the end initializes the swap table bits to true.
 * */
void init_swap_space(void)
{
    swap_disk=block_get_role(BLOCK_SWAP);

    if(swap_disk==NULL)
        PANIC("NO SWAP DISK FOUND");
    
    size_t bit_cnt=block_size(swap_disk)/NUM_SECTORS_PER_PAGE;
	swap_table=bitmap_create(bit_cnt);

	if(swap_table==NULL)
        PANIC("SWAP TABLE CANNOT BE CREATED");
    
	bitmap_set_all(swap_table,true);
}


/* Finds an empty slot in the swap table and write the page
 * represented by vaddr into the empty swap slot of the swap
 * disk.It returns a custom macro SWAP_ERROR if writing to the
 * swap disk fails. On success it returns the swap table slot.
 * */
size_t swap_out_page(void *vaddr)
{
    void* buffer;
    block_sector_t sector;
    size_t swap_slot=bitmap_scan_and_flip(swap_table,SWAP_TABLE_START,SWAP_TABLE_CNT,true);
    
    if (swap_slot==BITMAP_ERROR)
    return SWAP_ERROR;
    
    size_t counter=0;
    for(counter=0;counter<NUM_SECTORS_PER_PAGE;counter++)
    {
        sector=swap_slot*NUM_SECTORS_PER_PAGE+counter;
        buffer=vaddr+counter*BLOCK_SECTOR_SIZE;
        block_write(swap_disk,sector,buffer);
    }
    return swap_slot;
}

/* Swaps a page of data in a slot represented by swap_slot to
 * a page pointed by vaddr by reading(block_read) from the swap_disk.
 * Flip the corresponding swap slot bit in the swap table bitmap.
 * */
void swap_in_page(size_t swap_slot,void *vaddr)
{
	void* buffer;
	block_sector_t sector;
	size_t counter=0;
	for(counter=0;counter<NUM_SECTORS_PER_PAGE;counter++)
	{
		sector=swap_slot*NUM_SECTORS_PER_PAGE+counter;
		buffer=vaddr+counter*BLOCK_SECTOR_SIZE;
		block_read(swap_disk,sector,buffer);
	}
	bitmap_flip(swap_table,swap_slot);
}

/* Just flips the bit pointed by swap_slot.This is needed
 * when a page is freed from the supplementary table.
 * */
void swap_clear_slot(size_t swap_slot)
{
	bitmap_flip(swap_table,swap_slot);
}
