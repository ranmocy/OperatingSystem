#include "vm/frame.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/swap.h"

struct lock frame_table_lock;
struct list frame_table;

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
frame_table_init (void)
{
    list_init (&frame_table);
    lock_init (&frame_table_lock);
}

void *
frame_alloc (enum palloc_flags flags, struct SP_entry *page_entry)
{
    if ((flags & PAL_USER) == 0) {
        return NULL;
    }
    void *frame = palloc_get_page (flags);
    if (frame) {
        frame_add (frame, page_entry);
    } else {
        while (!frame) {
            frame = frame_evict (flags);
            lock_release (&frame_table_lock);
        }
        if (!frame) {
            PANIC ("Frame evict failed. Swap is full!");
        }
        frame_add (frame, page_entry);
    }
    return frame;
}

void
frame_free (void *frame)
{
    struct list_elem *e;

    lock_acquire (&frame_table_lock);
    for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e)) {
        struct frame_entry *frame_entry = list_entry (e, struct frame_entry, elem);
        if (frame_entry->frame == frame) {
            list_remove (e);
            free (frame_entry);
            palloc_free_page (frame);
            break;
        }
    }
    lock_release (&frame_table_lock);
}

void
frame_add (void *frame, struct SP_entry *page_entry)
{
    struct frame_entry *frame_entry = malloc (sizeof (struct frame_entry));
    frame_entry->frame = frame;
    frame_entry->page_entry = page_entry;
    frame_entry->thread = thread_current ();
    lock_acquire (&frame_table_lock);
    list_push_back (&frame_table, &frame_entry->elem);
    lock_release (&frame_table_lock);
}

void *
frame_evict (enum palloc_flags flags)
{
    lock_acquire (&frame_table_lock);
    struct list_elem *e = list_begin (&frame_table);

    while (true) {
        struct frame_entry *frame_entry = list_entry (e, struct frame_entry, elem);
        struct SP_entry *page_entry = frame_entry->page_entry;
        if (!page_entry->pinned) {
            struct thread *t = frame_entry->thread;
            if (pagedir_is_accessed (t->pagedir, page_entry->page)) {
                pagedir_set_accessed (t->pagedir, page_entry->page, false);
            } else {
                if (pagedir_is_dirty (t->pagedir, page_entry->page) ||
                    page_entry->type == SP_SWAP) {
                    if (page_entry->type == SP_MMAP) {
                        lock_acquire (&filesys_lock);
                        file_write_at (page_entry->file, frame_entry->frame,
                                       page_entry->read_bytes,
                                       page_entry->offset);
                        lock_release (&filesys_lock);
                    } else {
                        page_entry->type = SP_SWAP;
                        page_entry->swap_index = swap_out (frame_entry->frame);
                    }
                }
                page_entry->is_loaded = false;
                list_remove (&frame_entry->elem);
                pagedir_clear_page (t->pagedir, page_entry->page);
                palloc_free_page (frame_entry->frame);
                free (frame_entry);
                return palloc_get_page (flags);
            }
        }
        e = list_next (e);
        if (e == list_end (&frame_table)) {
            e = list_begin (&frame_table);
        }
    }
}
