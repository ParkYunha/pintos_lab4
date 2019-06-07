#include <stdbool.h>
#include <stddef.h>
#include <list.h>
#include "filesys/off_t.h"
#include "devices/disk.h"

#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H


/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
// added
#define MAX_DIRECT_BLOCKS 12
#define NUM_DIRECT_SECTORS 96 //12 * 8
#define NUM_INDIRECT_SECTORS 128 //(8+8)*8

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE = 512 bytes long. */
struct inode_disk
  {
    // disk_sector_t start;             /* First data sector. */ //no use for now
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[26];                /* Not used. -> to fit the size of inode_disk = 512 bytes */
    //added
    bool is_dir;                        /* True if it's a directory, false if not. */
    disk_sector_t parent;               /* which sector is this file(sector) (continued) from. */

    disk_sector_t direct_index[MAX_DIRECT_BLOCKS * 8];    /* Direct blocks (sectors * 8). */
    disk_sector_t indirect_index;                         /* Indirect block. */
    disk_sector_t double_indirect_index;                  /* Double indirect block. */
  };

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };


struct bitmap;


void inode_init (void);
bool inode_create (disk_sector_t, off_t, bool);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

// void inode_indexed_allocate(struct inode_disk *disk_inode);
// bool inode_grow(struct inode_disk *disk_inode, off_t length);
bool inode_grow_indirect(disk_sector_t* p_entry, size_t num_sectors, int level);

void inode_free(struct inode *inode);
void inode_free_indirect(disk_sector_t entry, size_t num_sectors, int level);

// disk_sector_t inode_index_to_sector(const struct inode_disk *idisk, off_t index);

#endif /* filesys/inode.h */
