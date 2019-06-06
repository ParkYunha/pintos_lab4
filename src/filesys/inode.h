#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include <stddef.h>
#include "filesys/off_t.h"
#include "devices/disk.h"

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
