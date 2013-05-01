#include "filesys/inode.h"
//6616
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include "threads/thread.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static int
byte_to_sector (const struct inode_disk *inode, off_t pos, bool create)
{
  ASSERT (inode != NULL);

  if(inode->start==2)
  {
    if (pos < inode->length)
    {
    	return inode->start + pos / BLOCK_SECTOR_SIZE;
    }
    else
      return -1;
  }


  int sector_number = (int) (pos / BLOCK_SECTOR_SIZE);
  int level1 = (int) (sector_number / 128);
  int level2 = (int) (sector_number % 128);


  block_sector_t arr[128];
  memset(arr,0,128*sizeof(block_sector_t));
  read_cache(inode->start,arr);

  if(arr[level1]==0)
  {
	  if(!create)
	  {
		  return -1;
	  }
	  if(!free_map_allocate(1,&arr[level1]))
			  return -1;
	  write_cache(inode->start,arr);
	  static char zero[512];
	  write_cache(arr[level1],zero);
  }


  block_sector_t l1=arr[level1];

  memset(arr,0,128*sizeof(block_sector_t));
  read_cache(l1,arr);
  if(arr[level2]==0)
  {
		  if(!create)
		  {
			  return -1;
		  }
	  if(!free_map_allocate(1,&arr[level2]))
		  return -1;
	  write_cache(l1,arr);
	  static char zero[512];
	  write_cache(arr[level2],zero);
  }
  return arr[level2];
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

static const char*
skipelem(const char *path, char *name)
{
  const char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= 14)
    memmove(name, s, 14);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

struct inode* inode_by_path(const char* path, bool parent)
{
  struct inode *ip, *next;
  char name[15]="";

  if(*path == '/')
       ip = inode_open(ROOT_DIR_SECTOR);
  else
  {
	  ip = thread_current()->cwd.inode;
	  if(ip==NULL)
		  return NULL;
	  ip->open_cnt++;
  }

  while((path = skipelem(path, name)) != 0){

    if(parent && *path == '\0'){
      // Stop one level early.
      return ip;
    }

      if(strcmp(".",name)==0)
      {
    	  next=ip;
    	  next->open_cnt++;
      }
      else if((next = inode_open(dir_lookup_by_inode(ip, name).inode_sector)) == NULL){
         return 0;
       }
       ip->open_cnt--;
       ip = next;
  }
  return ip;
}

bool inode_freemap_create (block_sector_t sector, off_t length)
{
	  struct inode_disk *disk_inode = NULL;
	  bool success = false;

	  /* If this assertion fails, the inode structure is not exactly
	     one sector in size, and you should fix that. */
	  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

	  disk_inode = calloc (1, sizeof *disk_inode);
	  if (disk_inode != NULL)
	    {
	      size_t sectors = bytes_to_sectors (length);
	      disk_inode->length = length;
	      disk_inode->magic = INODE_MAGIC;
	      disk_inode->isDir = false;

	      if (free_map_allocate (sectors, &disk_inode->start))
	        {
				write_cache(sector,disk_inode);
//	            {
//	              static char zero[BLOCK_SECTOR_SIZE];
//	              memset(zero,0,BLOCK_SECTOR_SIZE);
//              	  write_cache(disk_inode->start, zero);
//	            }
	          success = true;
	        }
	      free (disk_inode);
	    }
	  return success;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isDir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->isDir = isDir;

      if (free_map_allocate (1, &disk_inode->start))
        {
			write_cache(sector,disk_inode);
            {
              static char zero[BLOCK_SECTOR_SIZE];
              memset(zero,0,BLOCK_SECTOR_SIZE);
           	  write_cache(disk_inode->start, zero);
            }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{

	if(sector==666666)
		return NULL;
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */

  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  read_cache( inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }
      else
      {
    	  write_cache(inode->sector,&inode->data);
      }
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

//  int off=0;
//  while(off<size)
//  {
//	  int sector_idx = byte_to_sector (&inode->data, offset+off,false);
//	  if(sector_idx==-1)
//	  {
//		  off+=BLOCK_SECTOR_SIZE;
//		  continue;
//	  }
//	  add_read_ahead_sectors(sector_idx);
//	  off+=BLOCK_SECTOR_SIZE;
//  }

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      int sector_idx = byte_to_sector (&inode->data, offset,false);
      if(sector_idx==-1)
      {
    	  if(inode->data.length>=offset+size)
    	  {
    		  memset(buffer_,0,size);
    		  return bytes_read+size;
    	  }
    	  return 0;
      }
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
    	  break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          read_cache(sector_idx, buffer + bytes_read);
        }
      else read_cache_bounce(sector_idx, buffer + bytes_read, sector_ofs, chunk_size);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      int sector_idx = byte_to_sector (&inode->data, offset,true);
      if(sector_idx==-1)
    	  return bytes_written;
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          write_cache(sector_idx, buffer + bytes_written);
        }
      else write_cache_bounce(sector_idx,((void *)buffer) + bytes_written, sector_ofs, chunk_size);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  if(!inode->data.isDir&&inode->data.length<offset)
  {
	  inode->data.length=offset;
	  write_cache(inode->sector,&inode->data);
  }

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
