#include <stdbool.h>
#include "filesys/off_t.h"
#include "filesys/filesys.h"
#include "devices/disk.h"

#define MAX_CACHE_SIZE 64 /* Cache memory has at most 64 sectors */

/* 1 sectcor -> 1 unit */
struct cache_entry
{
  void *addr;    /* Memory address of the cached data. */
  disk_sector_t sector_num;   /* Disk sector number which this cached data came from. */ 
  bool has_data;      /* True if this entry has cached data. - valid bit */
  bool modified;     /* True if this data has been modified. - dirty bit */
  struct list_elem elem;    /* List element for buffer_cache list. */
};


//functions
void cache_init();
struct cache_entry *cache_search(disk_sector_t sector);
struct cache_entry *cache_get_free();
void cache_evict();

void cache_read(disk_sector_t sector, void *buffer);
void cache_write(disk_sector_t sector, void *buffer);

void cache_periodic_rewrite();
void cache_rewrite_disk();
