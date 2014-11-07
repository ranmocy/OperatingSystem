/*
 * Authors: Wanshang Sheng(ranmocy@gmail.com),
 *          Kaiming yang(yaxum62@gmail.com),
 *          Hao Chen(chenh1987@gmail.com)
 *
 * Version: 1.0.0
 *
 * Description: Supplemental Page Table
 *
 */

#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "vm/frame.h"

typedef struct hash SP_table_t;

typedef struct {
    void *page;
    struct FRAME_entry_t *frame;
    struct hash_elem elem;
} SP_entry_t;

void page_table_init (SP_table_t *page_table);
void page_table_destroy (SP_table_t *page_table);

SP_entry_t * page_find (SP_table_t *page_table, SP_entry_t *elem);
SP_entry_t * page_find_by_addr (SP_table_t *page_table, void *addr);
bool page_find_by_addr_and_load (SP_table_t *page_table, void *addr);

#endif
