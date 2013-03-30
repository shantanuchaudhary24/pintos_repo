#include <bitmap.h>

/* Defines the number of sectors needed for storing a page */
#define NUM_SECTORS_PER_PAGE PGSIZE/BLOCK_SECTOR_SIZE

/* Specifies the start of bitmap table for swap table*/
#define SWAP_TABLE_START 0

/* Specifies the cnt bit for scanning swap table*/
#define SWAP_TABLE_CNT 1

/* Error code returned by swap_out_page function on failure*/
#define SWAP_ERROR SIZE_MAX

void init_swap_space(void);
size_t swap_out_page(void *vaddr);
void swap_in_page(size_t swap_slot,void *vaddr);
void swap_clear_slot(size_t swap_slot);
