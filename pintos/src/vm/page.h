/*
 * Authors: Wanshang Sheng(ranmocy@gmail.com),
 *          Kaiming yang(yaxum62@gmail.com),
 *          Hao Chen(chenh1987@gmail.com)
 *
 * Version: 1.0.0
 *
 * Description: Page table
 *
 */

#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

enum page_t {
    SWAP,
    FILE,
    MMAP,
    ERROR
};

struct page_entry {
    enum page_t type;
    void *page;
    struct hash_elem elem;
};

// void page_table_init (struct hash *page_table);
// void page_table_destroy (struct hash *page_table);

// void page_add (enum palloc_flags flags, struct page_entry *page_entry);
// void page_free (void *page);

#endif
