#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
  };

struct dir_disk{
  block_sector_t parent;
  size_t n_entry;
};

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, block_sector_t parent, size_t entry_cnt)
{
  struct inode* inode;
  struct dir_disk dir;
  if (!inode_create (sector, entry_cnt * sizeof (struct dir_entry) + sizeof(struct dir_disk), TYPE_DIR))
    return false;
  dir.parent = parent;
  dir.n_entry = 0;
  inode = inode_open(sector);
  if (!inode)
    return false;
  inode_write_at(inode, &dir, sizeof(struct dir_disk), 0);
  inode_close(inode);
  return true;
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct file *
dir_open_root (void)
{
  return file_open (inode_open (ROOT_DIR_SECTOR));
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct file *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = sizeof(struct dir_disk); inode_read_at (file_get_inode(dir), &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.inode_sector != 0 && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct file *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (!filesys_isdir(dir))
    return false;


  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct file *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  if (!filesys_isdir(dir))
    return false;

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = sizeof(struct dir_disk); inode_read_at (file_get_inode(dir), &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.inode_sector == 0)
      break;
  /* Write slot. */
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (file_get_inode(dir), &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct file *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (!filesys_isdir(dir))
    return false;

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.inode_sector = 0;
  if (inode_write_at (file_get_inode(dir), &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct file *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  if (!filesys_isdir(dir))
    return false;

  if (file_tell(dir) == 0){
    file_seek(dir, sizeof(struct dir_disk));
  }
  while (inode_read_at (file_get_inode(dir), &e, sizeof e, file_tell(dir)) == sizeof e) 
    {
      file_seek(dir, file_tell(dir)+ sizeof e);
      if (e.inode_sector != 0)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

block_sector_t dir_parent (struct file* dir){
  struct dir_disk disk;
  ASSERT(filesys_isdir(dir));
  inode_read_at(file_get_inode(dir), &disk, sizeof(struct dir_disk), 0);
  return disk.parent;
      
}