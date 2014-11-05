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

typedef struct hash sup_page_table_t;

enum page_entry_type {
    SWAP,
    FILE,
    MMAP,
    ERROR
};

typedef struct {
    enum page_entry_type type;
    void *page;
    struct hash_elem elem;
} sup_page_entry_t;

void page_table_init (sup_page_table_t *page_table);
void page_table_destroy (sup_page_table_t *page_table);

void page_add (sup_page_table_t page_table, sup_page_entry_t *page_entry);
void page_free (void *page);

#endif
