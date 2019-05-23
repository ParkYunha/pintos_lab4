#include <stdbool.h>
#include "filesys/off_t.h"
#include "filesys/filesys.h"
#include "devices/disk.h"

#define MAX_CACHE_NUM 64 /* Cache memory has at most 64 sectors */

/* 1 sectcor -> 1 unit */
struct cache_entry
{
  void *paddr;    /* Memory address of the cached data. */
  disk_sector_t sector_num;   /* Sector number which this cached data came from. */ 
  bool valid_bit;     /* True if this entry has cached data. */
  bool dirty_bit;     /* True if this data has been modified. */
  struct list_elem elem;    /* List element for buffer_cache list. */
}


//functions
void buffer_cache_init();