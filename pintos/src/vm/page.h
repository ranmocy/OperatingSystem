#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "vm/frame.h"

#define MAX_STACK_SIZE (1 << 23) // 256 KB

enum SP_entry_type {
  SP_ERROR,
  SP_FILE,
  SP_SWAP,
  SP_MMAP
};

struct SP_entry {
  enum SP_entry_type type;
  void *page;

  // Flags
  bool is_loaded;
  bool writable;
  bool pinned;

  // File
  struct file *file;
  size_t offset;
  size_t read_bytes;
  size_t zero_bytes;

  // Swap
  size_t swap_index;

  struct hash_elem elem;
};

void page_table_init (struct hash *page_table);
void page_table_destroy (struct hash *page_table);

bool page_load (struct SP_entry *page_entry);
bool page_find (const void * vaddr);
bool page_find_and_load (const void * vaddr, const void * esp, const bool to_write);
bool page_add_file (struct file *file, int32_t ofs, uint8_t *upage,
                    uint32_t read_bytes, uint32_t zero_bytes,
                    bool writable);
bool page_add_mmap (struct file *file, int32_t ofs, uint8_t *upage,
                    uint32_t read_bytes, uint32_t zero_bytes);
bool grow_stack (const void *page);

#endif /* vm/page.h */
