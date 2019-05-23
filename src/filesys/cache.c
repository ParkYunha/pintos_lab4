#include <debug.h>
#include <string.h>
#include <list.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "threads/palloc.h"


struct list buffer_cache_list;
struct dist *filesys_disk;
struct bitmap *cache_map;
struct semaphore cache_sema;
// size_t cache_evict_num; //TODO: i dont think we need it

void buffer_cache_init()
{
  int i, j;
  for(i = 0; i < 8; ++i)
  {
    void* buffer_paddr = palloc_get_page(PAL_USER|PAL_ZERO);
    for(j = 0; j < 8; ++j)
    {
      struct cache_entry *cache;

      cache->paddr = buffer_paddr; //FIXME: j로 섹터 단위로 addr 포인터 조절해야겟다
      cache->valid_bit = false;
      cache->dirty_bit = false;

      list_push_back(buffer_cache_list, cache->elem);
    }
  }
}

/* Allocate new data to cache. */
/* If there is no false-valid(unused) entry in cache, evict one.*/
cache_get()

/* Read from cache instead of disk. */
cache_read_at()
{

}

/* Write at cache instead of disk. */
cache_write_at()


/* True if the data is in cache. */
cache_search()

/* Periodically rewrite the cache back to disk, using timer_sleep(). */
cache_periodic_rewrite()

/* Write back all the cached data to disk. */
cache_rewrite_disk()




