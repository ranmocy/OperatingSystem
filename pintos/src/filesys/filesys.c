#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "filesys/path.h"

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

  filesys_cache_init();
  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  char path[PATH_SIZE_LIMIT + 1], *fname;
  block_sector_t inode_sector = 0;
  struct file *dir;
  if (strlen(name) > PATH_SIZE_LIMIT)
    return false;
  strlcpy(path, name,PATH_SIZE_LIMIT);
  fname = path_split(path);
  dir = file_reopen(thread_current()->cur_dir);
  if (fname != NULL)
    dir = path_goto(dir, path);
  else
    fname = path;
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, TYPE_FILE)
                  && dir_add (dir, fname, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  file_close (dir);

  return success;
}

bool
filesys_mkdir(const char *name){
  char path[PATH_SIZE_LIMIT + 1], *fname;
  block_sector_t inode_sector = NULL_SECTOR;
  struct file *dir;
  if (strlen(name) > PATH_SIZE_LIMIT)
    return false;
  strlcpy(path, name,PATH_SIZE_LIMIT);
  fname = path_split(path);
  dir = file_reopen(thread_current()->cur_dir);
  if (fname != NULL)
    dir = path_goto(dir, path);
  else
    fname = path;

  if (dir != NULL 
      && free_map_allocate(1, &inode_sector)
      && dir_create(inode_sector,inode_get_inumber(file_get_inode(dir)), 0)
      && dir_add(dir, fname, inode_sector))
    return true;

  if (inode_sector!=NULL_SECTOR)
    free_map_release(inode_sector,1);
  return false;

}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char path[PATH_SIZE_LIMIT + 1], *fname;
  struct file *dir;
  if (strlen(name) > PATH_SIZE_LIMIT)
    return false;
  strlcpy(path, name,PATH_SIZE_LIMIT);
  fname = path_split(path);
  dir = file_reopen(thread_current()->cur_dir);
  if (fname != NULL)
    dir = path_goto(dir, path);
  else
    fname = path;

  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, fname, &inode);
  file_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char path[PATH_SIZE_LIMIT + 1], *fname;
  struct file *dir;
  if (strlen(name) > PATH_SIZE_LIMIT)
    return false;
  strlcpy(path, name,PATH_SIZE_LIMIT);
  fname = path_split(path);
  dir = file_reopen(thread_current()->cur_dir);
  if (fname != NULL)
    dir = path_goto(dir, path);
  else
    fname = path;
  bool success = dir != NULL && dir_remove (dir, fname);
  file_close (dir); 

  return success;
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

bool filesys_isdir(const struct file* f){
  return inode_get_flag(file_get_inode(f)) == TYPE_DIR;
}