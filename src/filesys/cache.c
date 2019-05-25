#include <debug.h>
#include <string.h>
#include <list.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "devices/timer.h"


struct list buffer_cache_list;
struct disk *filesys_disk;
struct bitmap *cache_map; //FIXME: we need it?
struct semaphore cache_sema;

void cache_init()
{
  int i;
  
  list_init(&buffer_cache_list);
  
  for(i = 0; i < MAX_CACHE_SIZE; ++i)
  {
    struct cache_entry *cache = calloc(1, sizeof (struct cache_entry));

    cache->addr = calloc (1, DISK_SECTOR_SIZE);
    cache->has_data = false;
    cache->sector_num = -1;
    cache->modified = false;
    list_push_back(&buffer_cache_list, &(cache->elem));
  }
  sema_init(&cache_sema, 1);
  thread_create("cache_rewrite", 0, cache_periodic_rewrite, NULL);
}


/* Find cache entry and return it, return null if cache miss. */
struct cache_entry*
cache_search(disk_sector_t sector)
{
  struct list_elem *e;
  struct cache_entry *cache;

  if(!list_empty(&buffer_cache_list))
  {
    for(e = list_begin(&buffer_cache_list); e != list_end(&buffer_cache_list); e = list_next(e))
    {
      cache = list_entry(e, struct cache_entry, elem);
      if(cache->sector_num == sector) //cache hit
      {
        return cache;
      }
    }
  }
  return NULL;  //cashe miss.
}


/* Find a empty(free) cache and return its entry_num. */
struct cache_entry*
cache_get_free() //function name 바꾸고 싶은데 뭐가 적절할까
{
  struct list_elem *e;
  struct cache_entry *cache;

  if(!list_empty(&buffer_cache_list))
  {
    for(e = list_begin(&buffer_cache_list); e != list_end(&buffer_cache_list); e = list_next(e))
    {
      cache = list_entry(e, struct cache_entry, elem);
      if(cache->has_data == false)
      {
        return cache;
      }
    }
  }
  return NULL; /* no empty sector in cache */
}


/* Evict first cache entry from the buffer_cache_list. */
void cache_evict()
{
  //FIFO
  struct cache_entry *cache;
  struct list_elem *e;
  
  e = list_pop_front(&buffer_cache_list);
  cache = list_entry(e, struct cache_entry, elem);
  if(cache->modified)
  {
    disk_write(filesys_disk, cache->sector_num, cache->addr);
  }
  free(cache->addr);
  free(cache);
}


/* Read from cache instead of disk. */
void cache_read(disk_sector_t sector, void *buffer)
{
  sema_down(&cache_sema);
  // printf("cache read(%d)\n", sector); //for debug

  struct cache_entry *find_cache = cache_search(sector);

  if(!find_cache)  //cache miss => fetch to cache from disk
  {
    struct cache_entry *new_cache = cache_get_free();
    if(new_cache==NULL)
    {
      cache_evict();
      new_cache = calloc(1, sizeof (struct cache_entry));
      new_cache->has_data = true;
      new_cache->modified = false;
      new_cache->sector_num = sector;
      new_cache->addr = malloc(DISK_SECTOR_SIZE);

      list_push_back(&buffer_cache_list, &new_cache->elem);
    }
    else
    {
      new_cache->has_data = true;
      new_cache->modified = false;
      new_cache->sector_num = sector;
      new_cache->addr = malloc(DISK_SECTOR_SIZE);
    }
    //fetch from disk
    disk_read(filesys_disk, new_cache->sector_num, new_cache->addr);
    //read from cache
    memcpy(buffer, new_cache->addr, DISK_SECTOR_SIZE);
  }
  else    //cache hit => read from cache
  {
    memcpy(buffer, find_cache->addr, DISK_SECTOR_SIZE);
    // find_cache->modified = true;
  }

  sema_up(&cache_sema);
}

/* Write at cache instead of disk. */
void cache_write(disk_sector_t sector, void *buffer)
{
  sema_down(&cache_sema);
  struct cache_entry *find_cache = cache_search(sector);

  if(!find_cache)  //cache miss => fetch to cache from disk
  {
    struct cache_entry *new_cache = cache_get_free();
    if(new_cache==NULL)
    {
      cache_evict();
      new_cache = calloc(1, sizeof (struct cache_entry));
      new_cache->has_data = true;
      new_cache->modified = false;
      new_cache->sector_num = sector;
      new_cache->addr = malloc(DISK_SECTOR_SIZE);
      // PANIC('write - 왜 뉴캐시가 없냐\n');

      list_push_back(&buffer_cache_list, &new_cache->elem);
    }
    else
    {
      new_cache->has_data = true;
      new_cache->modified = false;
      new_cache->sector_num = sector;
      new_cache->addr = malloc(DISK_SECTOR_SIZE);
    }
    //fetch to cache form disk
    disk_read(filesys_disk, new_cache->sector_num, new_cache->addr);
    //write at cache
    memcpy(new_cache->addr, buffer, DISK_SECTOR_SIZE);
    new_cache->modified = true;
  }
  else    //cache hit => write at cache
  {
    memcpy(find_cache->addr, buffer, DISK_SECTOR_SIZE);
    find_cache->modified = true;
  }

  sema_up(&cache_sema);
}


/* Periodically rewrite the cache back to disk, using timer_sleep(). */
void cache_periodic_rewrite()
{
  //busy waiting
  while(true)
  {
    timer_sleep(5 * TIMER_FREQ);
    cache_rewrite_disk();
  }
}

/* Write back all the cached data to disk. */
void cache_rewrite_disk()
{
  sema_down(&cache_sema);
  struct cache_entry *cache;
  struct list_elem *e;

  if(!list_empty(&buffer_cache_list))
  {
    for(e = list_begin(&buffer_cache_list); e != list_end(&buffer_cache_list); e = list_next(e))
    {
      cache = list_entry(e, struct cache_entry, elem);
      if(cache->modified == true)
      {
        disk_write(filesys_disk, cache->sector_num, cache->addr);
        cache->modified = false;
      }
    }
  }

  sema_up(&cache_sema);
}




