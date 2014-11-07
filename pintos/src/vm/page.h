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

typedef struct hash SP_table_t;
typedef struct SP_entry SP_entry_t;

#include <hash.h>
#include "vm/frame.h"

struct SP_entry {
    void *page;
    FRAME_entry_t *frame;
    struct hash_elem elem;
};

void page_table_init (SP_table_t *page_table);
void page_table_destroy (SP_table_t *page_table);

bool page_find_and_load_page (SP_table_t *page_table, void *page);
bool page_find_and_load_addr (SP_table_t *page_table, void *addr);

#endif
