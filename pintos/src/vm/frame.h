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

// Since it's double-linked entry between frame and page,
// pre-definition is required.
typedef struct frame_entry FRAME_entry_t;

#include <list.h>
#include "threads/palloc.h"
#include "vm/page.h"

struct frame_entry {
    void *frame;
    SP_entry_t *page_entry;
    struct list_elem elem;
};

void frame_init (void);

FRAME_entry_t *frame_create (enum palloc_flags flags, SP_entry_t *page_entry);
void frame_destroy (FRAME_entry_t *frame_entry);

#endif
