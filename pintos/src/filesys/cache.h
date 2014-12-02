#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include <list.h>

#define WRITE_BACK_INTERVAL 5*TIMER_FREQ
#define MAX_FILESYS_CACHE_SIZE 64
#define CACHE_MAGIC 0x31305750

struct list filesys_cache;
uint32_t filesys_cache_size;
struct lock filesys_cache_lock;


struct cache_entry {
  uint8_t block[BLOCK_SECTOR_SIZE]; //BLOCK_SECTOR_SIZE = 512
  unsigned magic;
  block_sector_t sector;            //index of the block_sector
  bool dirty;
  bool accessed;
  int counter;
  struct list_elem elem;
};

void filesys_cache_init (void);
struct cache_entry *block_in_cache (block_sector_t sector);
struct cache_entry* filesys_cache_block_get_read_only (block_sector_t sector);
struct cache_entry* filesys_cache_block_get_write (block_sector_t sector);
struct cache_entry* filesys_cache_block_evict (block_sector_t sector,
					       bool dirty);
void filesys_cache_write_to_disk (bool halt);
void thread_func_write_back (void *aux);
void set_dirty_flag(void*cache_elem);
void filesys_cache_block_release(void* ptr);
#endif /* filesys/cache.h */