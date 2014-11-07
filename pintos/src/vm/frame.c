#include "vm/frame.h"
#include <debug.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"

#define FRAME_ENTRY_SIZE sizeof(FRAME_entry_t)
#define TABLE_NAME frame_table

#define get_elem(FE_P)                  (&((FE_P)->elem))
#define get_frame_entry(ELEM_P)         list_entry ((ELEM_P), FRAME_entry_t, elem)

#define table_begin()                   get_frame_entry (list_begin (&TABLE_NAME))
#define table_end()                     get_frame_entry (list_end (&TABLE_NAME))
#define table_rbegin()                  get_frame_entry (list_rbegin (&TABLE_NAME))
#define table_rend()                    get_frame_entry (list_rend (&TABLE_NAME))
#define table_head()                    get_frame_entry (list_head (&TABLE_NAME))
#define table_tail()                    get_frame_entry (list_tail (&TABLE_NAME))

#define table_next(FE_P)                get_frame_entry (list_next (get_elem (FE_P)))
#define table_prev(FE_P)                get_frame_entry (list_prev (get_elem (FE_P)))

#define table_insert(FEE_P, FE_P)       list_insert (get_elem (FEE_P), get_elem (FE_P))
#define table_append(FEE_P, FE_P)       table_insert (table_next (FEE_P), FE_P)
#define table_push_front(FE_P)          list_push_front (&TABLE_NAME, get_elem (FE_P))
#define table_push_back(FE_P)           list_push_back (&TABLE_NAME, get_elem (FE_P))
#define table_remove(FE_P)              list_remove (get_elem (FE_P));

#define lock_table()                    lock_acquire (&frame_table_lock);
#define unlock_table()                  lock_release (&frame_table_lock);

struct list TABLE_NAME;
struct lock frame_table_lock;

void frame_add (void *frame, enum frame_entry_type type);
void * frame_evict (void);
bool load_swap (FRAME_entry_t *frame);
bool load_file (FRAME_entry_t *frame);
bool load_mmap (FRAME_entry_t *frame);


//
//                           ,,
//     `7MM"""Mq.            db                    mm
//       MM   `MM.                                 MM
//       MM   ,M9 `7Mb,od8 `7MM `7M'   `MF',6"Yb.mmMMmm .gP"Ya
//       MMmmdM9    MM' "'   MM   VA   ,V 8)   MM  MM  ,M'   Yb
//       MM         MM       MM    VA ,V   ,pm9MM  MM  8M""""""
//       MM         MM       MM     VVV   8M   MM  MM  YM.    ,
//     .JMML.     .JMML.   .JMML.    W    `Moo9^Yo.`Mbmo`Mbmmd'
//
//
void
frame_add (void *frame, enum frame_entry_type type)
{
    FRAME_entry_t *e = malloc (FRAME_ENTRY_SIZE);
    e->frame = frame;
    e->type = type;

    lock_table ();
    table_push_back (e);
    unlock_table ();
}

// evict oldest(first) element, and return the frame pointer
void *
frame_evict (void)
{
    // TODO: support swap
    PANIC ("No available SWAP to evict!");
}

bool
load_swap (FRAME_entry_t *frame_entry)
{
    // TODO
}

bool
load_file (FRAME_entry_t *frame_entry)
{
    // TODO
}

bool
load_mmap (FRAME_entry_t *frame_entry)
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
void frame_init (void)
{
    list_init (&frame_table);
    lock_init (&frame_table_lock);
}

void *
frame_alloc (enum palloc_flags flags)
{
    void *frame = palloc_get_page (flags);
    if (!frame) {
        frame = frame_evict ();
    }
    // evict should success or kernel panic
    ASSERT (frame != NULL);

    frame_add (frame, ERROR); // FIXME
    return frame;
}

void
frame_free (void *frame)
{
    FRAME_entry_t *e;
    bool found = false;

    lock_table ();
    for (e = table_begin (); e != table_end (); e = table_next (e)) {
        if (e->frame == frame) {
            table_remove (e);
            free (e);
            found = true;
            break;
        }
    }
    unlock_table ();

    if (!found) {
        PANIC ("Can't find the frame to free!");
    }

    palloc_free_page(frame);
}

bool
frame_load (FRAME_entry_t *frame_entry)
{
    switch (frame_entry->type) {
        case SWAP:
            return load_swap (frame_entry);
        case FILE:
            return load_file (frame_entry);
        case MMAP:
            return load_mmap (frame_entry);
        case ERROR:
            PANIC ("Can't load a frame with type ERROR!");
    }
}
