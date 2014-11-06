#include "page.h"
#include <debug.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"

#define get_elem(SPE_T)                 (&((SPE_T)->elem))
#define get_page_entry(ELEM_P)          hash_entry (ELEM_P, sup_page_entry_t, elem)

static unsigned
page_hash_func (const struct hash_elem *elem, void *aux UNUSED)
{
    return hash_int ((int) get_page_entry(elem)->page);
}

static bool
page_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
    return get_page_entry(a)->page < get_page_entry(b)->page;
}

static void
page_destroy_func (struct hash_elem *elem, void *aux UNUSED)
{
    sup_page_entry_t *entry = get_page_entry(elem);
    if (entry->page != NULL) {
        struct thread *t = thread_current ();
        frame_free (pagedir_get_page (t->pagedir, entry->page));
        pagedir_clear_page (t->pagedir, entry->page);
    }
    free (entry);
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
page_table_init (sup_page_table_t *page_table)
{
    hash_init (page_table, page_hash_func, page_less_func, NULL);
}

void
page_table_destroy (sup_page_table_t *page_table)
{
    hash_destroy (page_table, page_destroy_func);
}

void
page_add (sup_page_table_t page_table, sup_page_entry_t *page_entry)
{
    // TODO
}

void
page_free (void *page)
{
    // TODO
}
