#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "vm/page.h"

struct frame_entry {
    void *frame;
    struct SP_entry *page_entry;
    struct thread *thread;
    struct list_elem elem;
};

void frame_table_init (void);

void* frame_alloc (enum palloc_flags flags, struct SP_entry *page_entry);
void frame_free (void *frame);

void frame_add (void *frame, struct SP_entry *page_entry);
void* frame_evict (enum palloc_flags flags);

#endif /* vm/frame.h */
