/*
 * Authors: Wanshang Sheng(ranmocy@gmail.com),
 *          Kaiming yang(yaxum62@gmail.com),
 *          Hao Chen(chenh1987@gmail.com)
 *
 * Version: 1.0.0
 *
 * Description: Frame table
 *
 */

#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "vm/page.h"

/* Struct frame for physical memory */
struct frame_entry
{
    void *frame;
    struct page_entry *page_entry;
    struct list_elem elem;
};

void  frame_init (void);
void *frame_alloc (enum palloc_flags flags, struct page_entry *page_entry);
void  frame_free (void *frame);

#endif
