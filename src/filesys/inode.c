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

void print_block(block_sector_t sec)
{
	  block_sector_t arr[128];
	  //block_read(fs_device,sec,arr);
	  read_cache(sec,arr);
	  printf("here is the block %d\n",sec);
	  int i,j;
	  for(i=0;i<16;i++)
	  {
		  for(j=0;j<8;j++)
		  {
			  printf("%d, ",arr[i*8+j]);
		  }
		  printf("\n");
	  }

}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode_disk *inode, off_t pos, bool create)
{
  ASSERT (inode != NULL);
//  if (pos < inode->data.length)
//    return inode->start + pos / BLOCK_SECTOR_SIZE;
//  else
//    return -1;

  /*
   *
   *
   * NOTE: filesys-size=8 is a problem
   * NOTE: filesys-size=8 is a problem
   * NOTE: filesys-size=8 is a problem
   * NOTE: filesys-size=8 is a problem
   * NOTE: filesys-size=8 is a problem
   * NOTE: filesys-size=8 is a problem
   * NOTE: filesys-size=8 is a problem
   * NOTE: filesys-size=8 is a problem
   * NOTE: filesys-size=8 is a problem
   * NOTE: filesys-size=8 is a problem
   *
   *
   */

  if(inode->start==2)
  {
    if (pos < inode->length)
      return inode->start + pos / BLOCK_SECTOR_SIZE;
    else
      return -1;
  }

//  if(inode->isDir)
//  {
//	  return inode->start;
//  }

  int sector_number = (int) (pos / BLOCK_SECTOR_SIZE);
  int level1 = (int) (sector_number / 128);
  int level2 = (int) (sector_number % 128);

//  printf("sector number %d with start as %d requested with create as %d\n",sector_number, inode->start,create);

  block_sector_t arr[128];
  memset(arr,0,128*sizeof(block_sector_t));
  //block_read(fs_device,inode->start,arr);
  read_cache(inode->start,arr);
//  print_block(inode->start);

  if(arr[level1]==0)
  {
	  if(!create)
	  {
//		  printf("poooooops\n");
		  return -1;
	  }
	  if(!free_map_allocate(1,&arr[level1]))
			  return -1;
	  //block_write(fs_device,inode->start,arr);
	  write_cache(inode->start,arr);
	  //printf("arr[level1] new is %d\n",arr[level1]);
	  static char zero[512];
	  //block_write(fs_device,arr[level1],zero);
	  write_cache(arr[level1],zero);
  }

//  print_block(inode->start);

  block_sector_t l1=arr[level1];

  memset(arr,0,128*sizeof(block_sector_t));
  //block_read(fs_device,l1,arr);
  read_cache(l1,arr);
  if(arr[level2]==0)
  {
		  if(!create)
		  {
//			  printf("oooooops\n");
			  return -1;
		  }
	  if(!free_map_allocate(1,&arr[level2]))
		  return -1;
//	  printf("arr[level2] new is %d\n",arr[level2]);
	  //block_write(fs_device,l1,arr);
	  write_cache(l1,arr);
	  static char zero[512];
	  //block_write(fs_device,arr[level2],zero);
	  write_cache(l1,arr);
  }

//  printf("returning %d\n",arr[level2]);

//  print_block(inode->start);

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

static char*
skipelem(char *path, char *name)
{
  char *s;
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

struct inode* inode_by_path(char* path, bool parent)
{
//	printf("lookinf for inode by path %s, %d",path,parent);
  struct inode *ip, *next;
  char name[15]="";

  if(*path == '/')
       ip = inode_open(ROOT_DIR_SECTOR);
  else
  {
//	  printf("here in inode_by_path\n");
	  ip = thread_current()->cwd.inode;
	  if(ip==NULL)
		  return NULL;
	  ip->open_cnt++;

//	  printf("inode for cwd when searching for inode %x\n",ip);
  }

  while((path = skipelem(path, name)) != 0){
//     ilock(ip);
//     if(!ip->isDir){
////     iunlockput(ip);
//               printf("1 ip %x\n",ip);
//       return 0;
//     }

    if(parent && *path == '\0'){
      // Stop one level early.
//      iunlock(ip);
//         printf("ip %x\n",ip);
      return ip;
    }

//    printf("dir_lookingup by inode name %s, inode %x,sector %d, data start %d\n",name,ip,ip->sector,ip->data.start);
//    printf("dir looking up %s-%x, %s\n",path,ip, name);
      if(strcmp(".",name)==0)
      {
    	  next=ip;
    	  next->open_cnt++;
      }
      else if((next = inode_open(dir_lookup_by_inode(ip, name).inode_sector)) == NULL){
//       iunlockput(ip);
//                 printf("3 ip %x\n",ip);
         return 0;
       }
//       printf("found this %x\n",next);
       ASSERT(next->sector!=-1);
//     iunlockput(ip);
       ip->open_cnt--;
       ip = next;
  }
//  if(nameiparent){
//     iput(ip);
//     return 0;
//  }
//  printf("ip %x\n",ip);
  return ip;
}


/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isDir)
{
//	printf("INODE CREATING %d\n",sector);
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->isDir = isDir;

      if (free_map_allocate (1, &disk_inode->start))
        {
//          	write_cache(sector,disk_inode);
//    	  printf("writing new disk indoe in create to disk, %d\n",sizeof(*disk_inode));
//			block_write (fs_device, sector, disk_inode);
			write_cache(sector,disk_inode);
//    	  printf("back\n");
//          if (sectors > 0)
            {
//              static block_sector_t sects[BLOCK_SECTOR_SIZE/4];
              static char zero[BLOCK_SECTOR_SIZE];
              memset(zero,0,BLOCK_SECTOR_SIZE);
//              size_t i;
//
//              for (i = 0; i < sectors; i++) {
//            	  free_map_allocate(1, &sects[i]);
////            	  write_cache(disk_inode->start+i, zeros);
//              	  printf("writing zeroes to new disk_inodes start block\n");

              //  block_write (fs_device, disk_inode->start, zero);
              	  write_cache(disk_inode->start, zero);
//              }
//              block_write(fs_device, disk_inode->start, sects);
//              for (i = 0; i < sectors; i++) {
//            	  free_map_allocate(1, &sects[i]);
////            	  write_cache(disk_inode->start+i, zeros);
////				  block_write (fs_device, disk_inode->start + i*4, zeros);
//              }

//				int i=0;
//				for (;i<sectors;i++)
//				{
//					byte_to_sector(disk_inode,i*BLOCK_SECTOR_SIZE,true);
//				}
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
//  printf("inode open ka malloc %x\n",inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
//  read_cache ( inode->sector, &inode->data);
//  block_read (fs_device, inode->sector, &inode->data);
 read_cache( inode->sector, &inode->data);
  // printf("returned non null inode.\n");
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

//	printf("INODE BEING REMOVED\nINODE BEING REMOVED\nINODE BEING REMOVED\nINODE BEING REMOVED\n");
	//printf("closing inode %x\n",inode);
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
//	  printf("inode closing %x\n",inode);
      /* Remove from inode list and release lock. */
	 //printf("release resource blocks.\n");
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
			//printf("deallocating blocks.\n");
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      else
      {
//    	  block_write(fs_device,inode->sector,&inode->data);
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
  uint8_t *bounce = NULL;

//  if(size==0)
//	  printf("\n\n\n\n\nWHATSS\n");
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (&inode->data, offset,false);
      //printf("in read, byte to sector gives me %d, start sector is %d\n", sector_idx, inode->data.start);
      if(sector_idx==-1)
      {
    	  //printf("returning from read with -1 for sector %d, offset %d\n",inode->data.start,offset);
    	  if(inode->data.length>=offset+size)
    	  {
    		  memset(buffer_,0,size);
    		  return bytes_read+size;
    	  }
		  //printf("yahaan se return kar\n");
    	  return 0;
      }
	  //printf("Iske aage nahi jaunga\n");
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
//      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = sector_left;//inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
//      printf("size: %d, sector left %d\n",size,sector_left);
      if (chunk_size <= 0)
       {
//    	  printf("breaking here\n");
    	  break;
       }

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          read_cache(sector_idx, buffer + bytes_read);
//			block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          /*if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
              {
            	  printf("wow, malloc failed?!?!\n");
            	  break;
              }
            }
//          read_cache(sector_idx, bounce);
//          block_read (fs_device, sector_idx, bounce);
//          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
*/
//    	  printf("IN THIS HELLHOLE\n");
			read_cache_bounce(sector_idx, buffer + bytes_read, sector_ofs, chunk_size);
		}
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  //free (bounce);

//  printf("bytes read are %d\n",bytes_read);
//  printf("returned\n");
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
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
//		printf("entered inode_write, secotr %d, disk_sector %d\n",inode->sector, inode->data.start);
      block_sector_t sector_idx = byte_to_sector (&inode->data, offset,true);
//      printf("requested sector, got %d\n",sector_idx);
      if(sector_idx==-1)
    	  return bytes_written;
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
//      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = sector_left;//inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          write_cache(sector_idx, buffer + bytes_written);
//          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          // We need a bounce buffer.
    /*      if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

//           If the sector contains data before or after the chunk
//             we're writing, then we need to read in the sector
//             first.  Otherwise we start with a sector of all zeros. */
//          if (sector_ofs > 0 || chunk_size < sector_left)
//          	read_cache(sector_idx, bounce);
//            block_read (fs_device, sector_idx, bounce);
//          else
//            memset (bounce, 0, BLOCK_SECTOR_SIZE);
//          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
//          write_cache(sector_idx, bounce);
//		  block_write (fs_device, sector_idx, bounce);
//    	  printf("THIS OTHER HOLE\n");
        write_cache_bounce(sector_idx,((void *)buffer) + bytes_written, sector_ofs, chunk_size);
		
		}

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
 // free (bounce);

  if(!inode->data.isDir&&inode->data.length<offset+bytes_written)
  {
	  inode->data.length=offset;
//	  block_write(fs_device,inode->sector,&inode->data);
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
