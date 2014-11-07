/*
 * Authors: Wanshang Sheng(ranmocy@gmail.com),
 *          Kaiming yang(yaxum62@gmail.com),
 *          Hao Chen(chenh1987@gmail.com)
 *
 * Version: 1.0.0
 *
 * Description: Frame table is a linked list, ordered by latest usage time. Older first.
 *
 *
 */

#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "vm/page.h"

enum frame_entry_type {
    MEM,
    SWAP,
    FILE,
    MMAP,
    ERROR
};

typedef struct {
    enum frame_entry_type type;
    void *frame;
    struct SP_entry_t *page_entry;
    struct list_elem elem;
} FRAME_entry_t;

void  frame_init (void);
void *frame_alloc (enum palloc_flags flags);
void  frame_free (void *frame);

#endif
