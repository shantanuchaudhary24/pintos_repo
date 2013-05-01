#include "filesys/filesys.h"
//7227
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");
  inode_init ();
  free_map_init ();

  if (format) 
  {
	  do_format ();
  }

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  	flush_cache();
	free_map_close ();
}

char* filename_from_path(char* path)
{
		int i=0,mark=0;
       for(;i<strlen(path);i++)
       {
               if(path[i]=='/')
               {
            	   if(i+1<strlen(path)&&(path[i+1]!='.'||(i+2<strlen(path)&&path[i+2]=='.'&&path[i+1]=='.')))
            			   mark=i+1;
               }
       }

       return path+mark;
}


/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  if(strlen(filename_from_path(name))==0||strlen(filename_from_path(name))>14)
	  return false;
  struct inode* inode=inode_by_path(name,true);
  if(inode==NULL)
	  return false;
  struct dir* dir=dir_open(inode);
  ASSERT(dir);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, false)
                  && dir_add (dir, filename_from_path(name), inode_sector));

  struct inode* in;

  if (!success && inode_sector != 0) 
  {
	  free_map_release (inode_sector, 1);
  }
  dir_close (dir);
  return success;
}

bool filesys_create_folder (const char* name)
{
  if(strlen(filename_from_path(name))==0||strlen(filename_from_path(name))>14)
	  return false;

  struct inode* inode=inode_by_path(name,true);
  if(inode==NULL)
	  return NULL;
  struct dir* dir=dir_open(inode);

  block_sector_t inode_sector = 0;
  bool success = (dir != NULL
				  && free_map_allocate (1, &inode_sector)
				  && inode_create (inode_sector, 0, true)
				  && dir_add (dir, filename_from_path(name), inode_sector));
  if (!success && inode_sector != 0)
	free_map_release (inode_sector, 1);

  if(success)
  {
	  struct dir* dir_new=dir_open(inode_by_path(name,false));
	  ASSERT(dir_new);
	  dir_add(dir_new,"..",inode->sector);
	  dir_close(dir_new);
  }
  dir_close(dir);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct inode* inode=inode_by_path(name,false);//true
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  bool success = dir_remove (name);
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR))
    PANIC ("root directory creation failed");
  free_map_close ();
}
