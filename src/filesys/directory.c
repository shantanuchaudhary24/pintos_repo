#include "filesys/directory.h"
//24028
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector)
{
  return inode_create (sector, 0,true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

//  printf("now reading through various dir entries\n");
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
  {
//	printf("doing this shit\n");
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
//        printf("returning\n");
        return true;
      }
  }
//  printf("returning\n");
  return false;
}

//static bool readdir_entry (const struct inode *inode, const char *name)
//{
//  struct dir_entry e;
//  size_t ofs=inode->;
//
//  ASSERT (inode != NULL);
//  ASSERT (name != NULL);
//
////  printf("now reading through various dir entries\n");
//  if(inode_read_at (inode, &e, sizeof e, ofs) == sizeof e)
//      if (e.in_use)
//      {
//    	  strlcpy(name,e.name,15);
//    	  return true;
//      }
//  return false;
//}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
//	printf("now dir lookup\n");
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

struct dir_entry
dir_lookup_by_inode (const struct inode *inode, const char *name)//, struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

//  printf("dir_lookup %s\n",name);

  ASSERT (inode != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
  {
//	printf("read size: %d, sector %d\n",inode_read_at (inode, &e, sizeof e, 0),inode->data.start);
//    printf("Dir entry: %d, %s\n",e.in_use,e.name);

    if (e.in_use && !strcmp (name, e.name))
      {
//        if (ep != NULL)
//          *ep = e;
//        if (ofsp != NULL)
//          *ofsp = ofs;
        return e;//true;
      }
  }
//  for (ofs = 0; inode_read_at (inode, &e, sizeof e, ofs) == sizeof e;
//       ofs += sizeof e)
//  {
//	  printf("Dir entry: %d, %s\n",e.in_use,e.name);
//  }
//  printf("read size: %d, sector %d\n",inode_read_at (inode, &e, sizeof e, 0),inode->data.start);

//  printf("sending back a devil while looking for %s\n",name);
  e.inode_sector=666666;
  return e;//false;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
//	printf("entered dir add\n");
  struct dir_entry e;
  off_t ofs;
bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
  {
//	  printf("file exists\n");
	  goto done;
  }
//  else
//	  printf("lookup done, file doesnt exist yet\n");
  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
  {
//	  printf("name of directory entry is %s\n",e.name);
	  if (!e.in_use)
    {
//    	printf("normal break\n");
    	break;
    }
  }
  if(ofs==0)
  {
//	  printf("offset is zero\n");
	  memset(&e,0,sizeof(e));
  }

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
//  printf("inode write %x, ofd %d\n",dir->inode,ofs);
//  printf("inode write hee\n");
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
  if(success)
  {
//	  if(dir->inode->data.length<ofs+sizeof e)
	  {
		  dir->inode->data.length=ofs+sizeof e;
//		  printf("new size %d after making %s in %x\n",ofs+sizeof e,name,dir->inode);
		  block_write(fs_device,dir->inode->sector,&dir->inode->data);
	  }
  }
//	printf("exiting dir add %d\n",success);

 done:
// if(!success)
//	 printf("dir add failed, %d\n",ofs);
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (const char *name)
{
  struct dir* dir=dir_open(inode_by_path(name,true));

  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, filename_from_path(name), &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  block_read(fs_device,inode->sector,&inode->data);

  if(inode->data.isDir&&(inode->data.length!=sizeof(struct dir_entry)))
  {
	  printf("not deleting %d\n",inode->data.length);
	  goto done;
  }

  if(inode->sector==thread_current()->cwd.inode->sector)
  {
	  thread_current()->cwd.inode=NULL;
  }

  /* Erase directory entry. */
  e.in_use = false;
//  printf("and here\n");
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;
  block_read(fs_device,dir->inode->sector,&dir->inode->data);
//  printf("size was %d, %x\n",dir->inode->data.length,dir->inode);
  dir->inode->data.length-=sizeof e;
//  if(dir->inode->data.length==60)
//	  printf("yo\n");
  block_write(fs_device,dir->inode->sector,&dir->inode->data);
//  printf("size was %d, %x\n",dir->inode->data.length,dir->inode);

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use&&!strcmp("..",e.name)==0)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}
