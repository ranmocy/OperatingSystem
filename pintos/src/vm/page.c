#include "page.h"
#include <debug.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"

#define get_elem(SPE_P)             (&((SPE_P)->elem))
#define get_page_entry(ELEM_P)      hash_entry (ELEM_P, SP_entry_t, elem)

#define table_find(SPT_P, SPE_P)    get_page_entry (hash_find (SPT_P, get_elem (SPE_P)))


static unsigned
page_hash_func (const struct hash_elem *elem, void *aux UNUSED)
{
    void *p = get_page_entry(elem)->page;
    return hash_bytes (&p, sizeof(p));
}

static bool
page_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
    return get_page_entry(a)->page < get_page_entry(b)->page;
}

static void
page_destroy_func (struct hash_elem *elem, void *aux UNUSED)
{
    SP_entry_t *entry = get_page_entry(elem);
    if (entry->page != NULL) {
        struct thread *t = thread_current ();
        frame_free (pagedir_get_page (t->pagedir, entry->page));
        pagedir_clear_page (t->pagedir, entry->page);
    }
    free (entry);
}

void
page_add (SP_table_t page_table, SP_entry_t *page_entry)
{
    // TODO
}

void
page_free (void *page)
{
    // TODO
}


//
//                            ,,        ,,    ,,
//     `7MM"""Mq.            *MM      `7MM    db
//       MM   `MM.            MM        MM
//       MM   ,M9 `7MM  `7MM  MM,dMMb.  MM  `7MM  ,p6"bo
//       MMmmdM9    MM    MM  MM    `Mb MM    MM 6M'  OO
//       MM         MM    MM  MM     M8 MM    MM 8M
//       MM         MM    MM  MM.   ,M9 MM    MM YM.    ,
//     .JMML.       `Mbod"YML.P^YbmdP'.JMML..JMML.YMbmd'
//
//
void
page_table_init (SP_table_t *page_table)
{
    hash_init (page_table, page_hash_func, page_less_func, NULL);
}

void
page_table_destroy (SP_table_t *page_table)
{
    hash_destroy (page_table, page_destroy_func);
}

SP_entry_t *
page_find (SP_table_t *page_table, SP_entry_t *e)
{
    return table_find (page_table, e);
}

SP_entry_t *
page_find_by_addr (SP_table_t *page_table, void *page)
{
    SP_entry_t search_entry;
    search_entry.page = page;
    return page_find (page_table, &search_entry);
}

bool
page_find_by_addr_and_load (SP_table_t *page_table, void *page)
{
    SP_entry_t *e = page_find_by_addr (page_table, page);
    // TODO
    return false;
}
