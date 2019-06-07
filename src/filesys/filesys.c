#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"

#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/init.h"
/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");
  cache_init();
  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();

  /* working dir setting */
  thread_current()->dir = dir_open_root();
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
filesys_create (const char *name, off_t initial_size, bool is_dir)
{
  disk_sector_t inode_sector = 0;

  char *name_ = palloc_get_page(0);
  char *file_name = palloc_get_page(0);
  strlcpy (name_, name, PGSIZE);


  struct dir *dir = path_to_dir(name_, file_name);
  bool success = false;

  if(strcmp(file_name, ".") != 0 && strcmp(file_name, "..")!=0)
  {
    success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, file_name, inode_sector));
  }
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

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
  char *name_ = palloc_get_page(0);
  char *file_name = palloc_get_page(0);
  strlcpy(name_, name, PGSIZE);

  if (name_ == NULL || file_name == NULL)
    return NULL;

  struct dir *dir = path_to_dir(name_, file_name);
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  char *name_ = palloc_get_page(0);
  char *file_name = palloc_get_page(0);
  strlcpy(name_, name, PGSIZE);

  struct dir *dir = path_to_dir(name_, file_name);

  bool success = dir != NULL && dir_remove (dir, file_name);
  //dir_close (dir);
  /*
  bool success = false;
  struct inode *inode;
  if(file_name != NULL)
  {
    if(dir_lookup(dir, file_name, &inode))
    {
      bool inode_dir;
      struct inode_disk *disk_inode = NULL;
      disk_inode = calloc(1, sizeof *disk_inode);
      if(disk_inode == NULL || inode == NULL)
        inode_dir = false;
      else
        {
          cache_read(inode, disk_inode); //?
          if (disk_inode->is_dir == true)
            inode_dir = true;
          else
            inode_dir = false;
          free(disk_inode);
        }
      if(inode_dir)
      {
        struct dir *remove_dir = dir_open(inode);
        if(!dir_readdir(remove_dir, file_name))
          success = dir_remove(dir, file_name);
      }
      else
        success = dir_remove(dir, file_name);
    }
  }
*/
  dir_close(dir);
  palloc_free_page(name_);
  palloc_free_page(file_name);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  struct dir *root = dir_open_root();
  dir_add(root, ".", ROOT_DIR_SECTOR);
  dir_close(root);
  free_map_close ();
  printf ("done.\n");
}

/* parse the path and return proper dir */
struct dir*
path_to_dir(char *path_name, char *file_name)
{
  struct dir *dir;
  struct thread *cur_t = thread_current();
  char *tok;
  char *ntok;
  char *ptr;


  if (path_name == NULL || file_name == NULL)
    return NULL;
  if(strlen(path_name) == 0)
    return NULL;

    /*parse path_name and find dir*/
    tok = strtok_r(path_name, "/", &ptr);
    ntok = strtok_r(NULL, "/", &ptr);
    if (*path_name == "/")
      dir = dir_open_root();
    else
    {
      if (cur_t->dir == NULL)
        dir = dir_open_root();
      else
        {
          dir = dir_reopen(cur_t->dir);
        }
    }

    /*this inode must dir*/
    struct inode *inode;

    while(tok != NULL && ntok != NULL)
    {
      if(dir_lookup(dir, tok, &inode))
      {
        bool inode_dir;
        struct inode_disk *disk_inode = NULL;
        disk_inode = calloc(1, sizeof *disk_inode);
        if(disk_inode == NULL || inode == NULL)
          inode_dir = false;
        else
          {
            cache_read(inode, disk_inode); //?
            if (disk_inode->is_dir == true)
              inode_dir = true;
            else
              inode_dir = false;
            free(disk_inode);
          }
        if(inode_dir)
        {
          dir = dir_open(inode);
          tok = ntok;
          ntok = strtok_r(NULL, "/", &ptr);
        }
        else
        {
          dir_close(dir);
          return NULL;
        }
      }
      else
      {
        dir_close(dir);
        return NULL;
      }
    }

    if(tok != NULL)
      strlcpy(file_name, tok, strlen(tok)+1);

    return dir;
}
