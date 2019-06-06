#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
// added
#define DIRECT_BLOCKS_COUNT 96 //12 * 8
#define MAX_DIRECT_BLOCKS 12
#define INDIRECT_PTR_PER_BLOCK 128 //(8+8)*8

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE = 512 bytes long. */
struct inode_disk
  {
    // disk_sector_t start;                /* First data sector. */ //no use for now
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[26];               /* Not used. */
    //added
    // size_t alloc_sectors;               /* Number of the allocated sectors. */
    bool is_dir;                        /* True if it's a directory, false if not. */
    disk_sector_t parent;               /* which sector is this file(sector) (continued) from. */

    disk_sector_t direct_index[MAX_DIRECT_BLOCKS * 8];         /* Direct blocks. */      
    disk_sector_t indirect_index;            /* Indirect block. */
    disk_sector_t double_indirect_index;     /* Double indirect block. */
    // disk_sector_t sectors[14];          /* Total sectors. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

static inline size_t
MIN (size_t a, size_t b)
{
  return a < b ? a : b;
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    //added.     
    // uint32_t direct_index;              /* Direct sector. */
    // uint32_t indirect_index;            /* Indirect sector. */
    // uint32_t double_indirect_index;     /* Double indirect sector. */
    // disk_sector_t sectors[14];          /* Total sectors. */
  };

bool inode_indexed_allocate(struct inode_disk *disk_inode);
bool inode_grow(struct inode_disk *disk_inode, off_t length);
disk_sector_t inode_index_to_sector(const struct inode_disk *idisk, off_t index);


/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
  {
    // return inode->data.start + pos / DISK_SECTOR_SIZE;  //base filesytem
    off_t index = pos / DISK_SECTOR_SIZE;
    return inode_index_to_sector(&inode->data, index);
  }
  else
    return -1;
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

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  // PANIC("*** %d, %d *** \n", sizeof *disk_inode, DISK_SECTOR_SIZE);
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);
  

  disk_inode = calloc (1, sizeof(struct inode_disk));
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = is_dir;
      disk_inode->parent = ROOT_DIR_SECTOR;

      if(inode_indexed_allocate(disk_inode))
      {
        cache_write(sector, disk_inode);
        success = true;
      }


      // if (free_map_allocate (sectors, &disk_inode->start))
      //   {
      //     // disk_write (filesys_disk, sector, disk_inode);
      //     // printf("create length %u | %u\n",length,disk_inode->length); //debug
      //     // printf("create write to buffer | addr %p\n",disk_inode);     //debug
      //     cache_write(sector, disk_inode);
      //     if (sectors > 0) 
      //       {
      //         static char zeros[DISK_SECTOR_SIZE];
      //         size_t i;
              
      //         for (i = 0; i < sectors; i++) 
      //           // disk_write (filesys_disk, disk_inode->start + i, zeros); 
      //           cache_write(disk_inode->start + i, zeros);
      //       }
      //     success = true; 
      //   } 

      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
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
  // disk_read (filesys_disk, inode->sector, &inode->data);
  // printf("inode_open(%d)\n", sector);
  cache_read(inode->sector, &inode->data);

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
disk_sector_t
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
          // free_map_release (inode->data.start,
          //                   bytes_to_sectors (inode->data.length));   //base filesystem
          inode_free(inode);
        }

      free (inode);  //TODO: still need it?
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

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Read full sector directly into caller's buffer. */
          // disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
          cache_read(sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          // disk_read (filesys_disk, sector_idx, bounce);
          cache_read(sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

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

  //extensible filesystem
  if(byte_to_sector(inode, offset + size - 1) == -1)
  {
    bool success;
    success = inode_grow (& inode->data, offset + size);
    if (!success){
      return 0;
    }

    // write back the (extended) file size
    inode->data.length = offset + size;
    cache_write (inode->sector, & inode->data);
  }

//  printf("len %u | add %p\n",inode_length(inode),inode);    //debug
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;

      // printf("\n inode %p | buf %p | size %u | offs %u\n",inode, buffer_, size, offset);
      // printf("len %u | sec ofs %u\n",inode_length(inode),sector_ofs);
      // printf("chunk %d | min_left %d | inode_left %d | sector_left %d\n",chunk_size,min_left,inode_left,sector_left);
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Write full sector directly to disk. */
          // disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
          cache_write(sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            // disk_read (filesys_disk, sector_idx, bounce);
            cache_read(sector_idx, bounce);
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          // disk_write (filesys_disk, sector_idx, bounce); 
          cache_write(sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

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

/* Allocate memory to inode_disk. */
bool
inode_indexed_allocate(struct inode_disk *disk_inode)
{
  // struct inode inode;
  // inode.direct_index = 0;
  // inode.indirect_index = 0;
  // inode.double_indirect_index = 0;

  // inode_indexed_grow(&inode, disk_inode->length);
  // disk_inode->direct_index = inode.direct_index;
  // disk_inode->indirect_index = inode.indirect_index;
  // disk_inode->double_indirect_index = inode.double_indirect_index;

  // memcpy(&disk_inode->sectors, &inode.sectors, 14 * sizeof(disk_sector_t));

  return inode_grow(disk_inode, disk_inode->length);
}


bool
inode_grow(struct inode_disk *disk_inode, off_t length)
{
  static char zeros[DISK_SECTOR_SIZE] = {0,};

  // printf("inode_disk(%d)\n", length); //debug

  if(length < 0) {
    return false;
  }

  size_t used_sectors = bytes_to_sectors(length);
  size_t len;
  int i;

  // (1) direct sectors
  len = MIN(used_sectors, 12 * 8); //FIXME: 
  for(i = 0; i < len; ++i)
  {
    if(disk_inode->direct_index[i] == 0) { // unoccupied
      if(! free_map_allocate (1, &disk_inode->direct_index[i]))
        return false;
      cache_write (disk_inode->direct_index[i], zeros);
    }
  }
  used_sectors -= len;
  if(used_sectors == 0) {
    return true;
  }

  // for(i = 0 ; i < len ; ++i)
  //   printf("inode_grow - direct(%d)\n", disk_inode->direct_index[i]);

  // (2) single indirect sector
  len = MIN(used_sectors, 16 * 8);     //FIXME: 
  if(! inode_grow_indirect (& disk_inode->indirect_index, len, 1))
    return false;
  used_sectors -= len;
  if(used_sectors == 0) return true;


  // (3) double indirect sector
  len = MIN(used_sectors, (16 * 8) * (16 * 8));    //FIXME: 
  if(! inode_grow_indirect (& disk_inode->double_indirect_index, len, 2))
    return false;
  used_sectors -= len;
  if(used_sectors == 0) return true;


  //failed
  return false;
}

bool
inode_grow_indirect(disk_sector_t* p_entry, size_t num_sectors, int level)
{
  
  static char zeros[DISK_SECTOR_SIZE] = {0,};

  if (level == 0) {
    // base case : allocate a single sector if necessary and put it into the block
    if (*p_entry == 0) {
      if(! free_map_allocate (1, p_entry))
        return false;
      cache_write (*p_entry, zeros);
    }
    return true;
  }

  disk_sector_t indirect_blocks[128] = {0,}; //FIXME: 128 right?
  if(*p_entry == 0) {
    // not yet allocated: allocate it, and fill with zero
    free_map_allocate (1, p_entry);
    cache_write (*p_entry, zeros);
  }
  cache_read(*p_entry, &indirect_blocks);
  // printf("inode_grow_indirect %d %d %d\n", *p_entry, num_sectors, level); //debug

  size_t unit = (level == 1 ? 1 : 128);
  size_t len = DIV_ROUND_UP (num_sectors, unit);
  int i;

  for (i = 0; i < len; ++ i) {
    size_t subsize = MIN(num_sectors, unit);
    indirect_blocks[i] = 0;
    if(! inode_grow_indirect (& indirect_blocks[i], subsize, level - 1)) //recursive
      return false;
    num_sectors -= subsize;
  }

  for(i = 0; i < len; ++i) {
    // printf("indirect created(%d)\n", indirect_blocks[i]); //debug
  }

  ASSERT (num_sectors == 0);
  cache_write (*p_entry, &indirect_blocks);
  return true;

}


/* Deallocate the inode. */
void
inode_free(struct inode *inode)
{
  off_t file_length = inode->data.length;   //size in bytes
  if(file_length < 0){
     return false;
  }

  // (remaining) number of sectors, occupied by this file.
  size_t num_sectors = bytes_to_sectors(file_length);
  size_t i, len;

  // (1) direct blocks
  len = MIN(num_sectors, 12 * 8); //FIXME: 
  for (i = 0; i < len; ++ i) {
    free_map_release (inode->data.direct_index[i], 1);
  }
  num_sectors -= len;


  // (2) indirect block
  len = MIN(num_sectors, 16 * 8); //FIXME: 
  if(len > 0) {
    inode_free_indirect (inode->data.indirect_index, len, 1);
    num_sectors -= len;
  }

  // (3) doubly indirect block
  len = MIN(num_sectors, (16 * 8) * (16 * 8));
  if(len > 0) {
    inode_free_indirect (inode->data.double_indirect_index, len, 2);
    num_sectors -= len;
  }

  ASSERT (num_sectors == 0);
  return true;

}

void
inode_free_indirect(disk_sector_t entry, size_t num_sectors, int level)
{
  ASSERT (level <= 2);

  if (level == 0) {
    free_map_release (entry, 1);
    return;
  }

  disk_sector_t indirect_blocks[128];
  cache_read(entry, &indirect_blocks);

  size_t unit = (level == 1 ? 1 : INDIRECT_PTR_PER_BLOCK);
  size_t i, len = DIV_ROUND_UP (num_sectors, unit);

  for (i = 0; i < len; ++ i) {
    size_t subsize = MIN(num_sectors, unit);
    inode_free_indirect (indirect_blocks[i], subsize, level - 1);
    num_sectors -= subsize;
  }

  ASSERT (num_sectors == 0);
  free_map_release (entry, 1);


}


struct inode_indirect_block_sector {
  disk_sector_t blocks[INDIRECT_PTR_PER_BLOCK];
};

/* Helper function for byte_to_sector() */
disk_sector_t
inode_index_to_sector(const struct inode_disk *idisk, off_t index)
{
  off_t index_base = 0, index_limit = 0;   // base, limit for sector index
  disk_sector_t ret;
  int i;

  // (1) direct blocks
  // printf("direct\n"); //debug
  index_limit += DIRECT_BLOCKS_COUNT * 1;
  if (index < index_limit) {
    // printf(" **** index: %d, limit: %d, ret: %d \n", index, index_limit, idisk->direct_index[index]); //debug
    return idisk->direct_index[index];
  }
  //else: need more space after direct blocks -> indirect blocks
  index_base = index_limit;

  // (2) a single indirect block
  index_limit += INDIRECT_PTR_PER_BLOCK;
  if (index < index_limit) {
    // printf(" **** single indirect\n"); //debug
    struct inode_indirect_block_sector *indirect_idisk;
    // for(i = 0; i < INDIRECT_PTR_PER_BLOCK; ++i){
    //   indirect_idisk->blocks[i] = -1;
    // }

    indirect_idisk = calloc(1, sizeof(struct inode_indirect_block_sector));
    cache_read (idisk->indirect_index, indirect_idisk);
    // printf("--- cache.. indirect index: %d\n", idisk->indirect_index); //debug

    // printf("---ret---\n");
    // for(i=0; i<INDIRECT_PTR_PER_BLOCK; ++i){
    //   printf(indirect_idisk->blocks[i]);
    // }

    ret = indirect_idisk->blocks[ index - index_base ];
    free(indirect_idisk);
    // printf(" **** ret: %d, index: %d, base: %d \n", ret, index, index_base); //debug

    return ret;
  }
  //else: need more space -> doubly indirect block
  index_base = index_limit;

  // (3) a single doubly indirect block
  index_limit += 1 * INDIRECT_PTR_PER_BLOCK * INDIRECT_PTR_PER_BLOCK;
  if (index < index_limit) {
    // printf(" **** double-indirect\n"); //debug
    // first and second level block index, respecitvely
    off_t index_first =  (index - index_base) / INDIRECT_PTR_PER_BLOCK;
    off_t index_second = (index - index_base) % INDIRECT_PTR_PER_BLOCK;

    // fetch two indirect block sectors
    struct inode_indirect_block_sector *indirect_idisk;
    indirect_idisk = calloc(1, sizeof(struct inode_indirect_block_sector));

    cache_read (idisk->double_indirect_index, indirect_idisk);
    cache_read (indirect_idisk->blocks[index_first], indirect_idisk);
    ret = indirect_idisk->blocks[index_second];

    free(indirect_idisk);
    return ret;
  }

  // (4) what up?
  return -1;

}