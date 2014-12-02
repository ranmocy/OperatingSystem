#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"

void filesys_cache_init (void)
{
  list_init(&filesys_cache);
  lock_init(&filesys_cache_lock);
  filesys_cache_size = 0;
  //thread_create("filesys_cache_writeback", 0, thread_func_write_back, NULL);
}

struct cache_entry* block_in_cache (block_sector_t sector)
{
  struct cache_entry *cache_elem;
  struct list_elem *e;
  for (e = list_begin(&filesys_cache); e != list_end(&filesys_cache);
       e = list_next(e))
    {
      cache_elem = list_entry(e, struct cache_entry, elem);
      if (cache_elem->sector == sector)
	    {
	      return cache_elem;
	    }
    }
  return NULL;
}

struct cache_entry* filesys_cache_block_get_read_only (block_sector_t sector)
{
  lock_acquire(&filesys_cache_lock);
  struct cache_entry *cache_elem = block_in_cache(sector);
  if (cache_elem)
    {
      cache_elem->counter++;
      cache_elem->accessed = true;
      lock_release(&filesys_cache_lock);
      cache_elem->magic=CACHE_MAGIC;
      return cache_elem;
    }
  cache_elem = filesys_cache_block_evict(sector, false);
  if (!cache_elem)
    {
      PANIC("Not enough memory for buffer cache.");
    }
  lock_release(&filesys_cache_lock);
  cache_elem->magic=CACHE_MAGIC;
  return cache_elem;
}

struct cache_entry* filesys_cache_block_get_write (block_sector_t sector)
{
  lock_acquire(&filesys_cache_lock);
  struct cache_entry *cache_elem = block_in_cache(sector);
  if (cache_elem)
    {
      cache_elem->counter++;
      cache_elem->dirty = true;
      cache_elem->accessed = true;
      lock_release(&filesys_cache_lock);
      cache_elem->magic=CACHE_MAGIC;
      return cache_elem;
    }
  cache_elem = filesys_cache_block_evict(sector, true);
  if (!cache_elem)
    {
      PANIC("Not enough memory for buffer cache.");
    }
  lock_release(&filesys_cache_lock);
  cache_elem->magic=CACHE_MAGIC;
  return cache_elem;
}



struct cache_entry* filesys_cache_block_evict (block_sector_t sector, bool dirty)
{
  struct cache_entry *cache_elem;
  if (filesys_cache_size < MAX_FILESYS_CACHE_SIZE)
  {
    cache_elem = malloc(sizeof(struct cache_entry));
    if (!cache_elem)
	  {
      return NULL;
    }
    cache_elem->counter = 0;
    list_push_back(&filesys_cache, &cache_elem->elem);
    filesys_cache_size++;
  }
  else
  {
    bool loop = true;
    while (loop)
    {
      struct list_elem *e;
      for (e = list_begin(&filesys_cache); e != list_end(&filesys_cache);
	         e = list_next(e))
      {
        cache_elem = list_entry(e, struct cache_entry, elem);
        if (cache_elem->counter > 0)
		    {
		      continue;
		    }
        if (cache_elem->accessed)
		    {
		      cache_elem->accessed = false;
		    }
        else
		    {
		      if (cache_elem->dirty)
		      {
		        block_write(fs_device, cache_elem->sector, &cache_elem->block);
		      }
		      loop = false;
		      break;
		    }
	    }
	  }
  }
  cache_elem->counter++;
  cache_elem->sector = sector;
  block_read(fs_device, cache_elem->sector, &cache_elem->block);
  cache_elem->dirty = dirty;
  cache_elem->accessed = true;
  return cache_elem;
}

void filesys_cache_write_to_disk (bool halt)
{
  printf("========================================\n\
    ===============SAVE==========================\n\
    =============================================");
  lock_acquire(&filesys_cache_lock);
  struct list_elem *next, *e = list_begin(&filesys_cache);
  while (e != list_end(&filesys_cache))
    {
      next = list_next(e);
      struct cache_entry *cache_elem = list_entry(e, struct cache_entry, elem);
      if (cache_elem->accessed)
      {
        cache_elem->accessed = false;
        continue;
      }
      if (cache_elem->dirty)
	    {
	      block_write (fs_device, cache_elem->sector, &cache_elem->block);
	      cache_elem->dirty = false;
	    }
      if (halt)
	    {
	      list_remove(&cache_elem->elem);
	      free(cache_elem);
	    }
      e = next;
    }
  lock_release(&filesys_cache_lock);
}

void thread_func_write_back (void *aux UNUSED)
{
  while (true)
    {
      timer_sleep(WRITE_BACK_INTERVAL);
      filesys_cache_write_to_disk(true);
    }
}

void set_dirty_flag(void *cache_elem)
{
  ASSERT(((struct cache_entry*)cache_elem)->magic == CACHE_MAGIC);
	((struct cache_entry*)cache_elem)->dirty = true;
}

void filesys_cache_block_release(void* ptr){
  ASSERT(((struct cache_entry*)ptr)->magic == CACHE_MAGIC);
	((struct cache_entry*)ptr)->counter--;
}

void cache_read(block_sector_t sector, void *buffer){
	struct cache_entry* ptr = filesys_cache_block_get_read_only(sector);
	memcpy(buffer, ptr->block, (BLOCK_SECTOR_SIZE));
	filesys_cache_block_release(ptr);
}

void cache_write(block_sector_t sector, void *buffer){
	struct cache_entry* ptr = filesys_cache_block_get_write(sector);
	memcpy(ptr->block, buffer, (BLOCK_SECTOR_SIZE));
	filesys_cache_block_release(ptr);
}