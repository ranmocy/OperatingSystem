#include <string.h>
#include <stdbool.h>
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

//
//                         ,,
//   `7MM"""Mq.            db                    mm
//     MM   `MM.                                 MM
//     MM   ,M9 `7Mb,od8 `7MM `7M'   `MF',6"Yb.mmMMmm .gP"Ya
//     MMmmdM9    MM' "'   MM   VA   ,V 8)   MM  MM  ,M'   Yb
//     MM         MM       MM    VA ,V   ,pm9MM  MM  8M""""""
//     MM         MM       MM     VVV   8M   MM  MM  YM.    ,
//   .JMML.     .JMML.   .JMML.    W    `Moo9^Yo.`Mbmo`Mbmmd'
//
//

static unsigned
page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
    struct SP_entry *page_entry = hash_entry(e, struct SP_entry, elem);
    return hash_int((int) page_entry->page);
}

static bool
page_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
    struct SP_entry *sa = hash_entry(a, struct SP_entry, elem);
    struct SP_entry *sb = hash_entry(b, struct SP_entry, elem);
    if (sa->page < sb->page) {
        return true;
    }
    return false;
}

static void
page_action_func (struct hash_elem *e, void *aux UNUSED)
{
    struct SP_entry *page_entry = hash_entry(e, struct SP_entry, elem);
    if (page_entry->is_loaded) {
        frame_free (pagedir_get_page (thread_current()->pagedir, page_entry->page));
        pagedir_clear_page (thread_current()->pagedir, page_entry->page);
    }
    free(page_entry);
}

static struct SP_entry*
get_page_entry (const void *page)
{
    struct SP_entry page_entry;
    page_entry.page = pg_round_down(page);

    struct hash_elem *e = hash_find (&thread_current()->page_table, &page_entry.elem);
    if (!e) {
        return NULL;
    }
    return hash_entry (e, struct SP_entry, elem);
}


static bool
load_swap (struct SP_entry *page_entry)
{
    uint8_t *frame = frame_alloc (PAL_USER, page_entry);
    if (!frame) {
        return false;
    }
    if (!install_page (page_entry->page, frame, page_entry->writable)) {
        frame_free (frame);
        return false;
    }
    swap_in (page_entry->swap_index, page_entry->page);
    page_entry->is_loaded = true;
    return true;
}

static bool
load_file (struct SP_entry *page_entry)
{
    void *frame = frame_alloc (PAL_USER, page_entry);
    if (!frame) {
        return false;
    }

    if (page_entry->read_bytes > 0) {
        lock_acquire (&filesys_lock);
        if ((int) page_entry->read_bytes != file_read_at (page_entry->file, frame, page_entry->read_bytes, page_entry->offset)) {
            lock_release (&filesys_lock);
            frame_free (frame);
            return false;
        }
        lock_release (&filesys_lock);
    }

    memset (frame + page_entry->read_bytes, 0, page_entry->zero_bytes);

    if (!install_page (page_entry->page, frame, page_entry->writable)) {
        frame_free (frame);
        return false;
    }

    page_entry->is_loaded = true;
    return true;
}

//
//                          ,,        ,,    ,,
//   `7MM"""Mq.            *MM      `7MM    db
//     MM   `MM.            MM        MM
//     MM   ,M9 `7MM  `7MM  MM,dMMb.  MM  `7MM  ,p6"bo
//     MMmmdM9    MM    MM  MM    `Mb MM    MM 6M'  OO
//     MM         MM    MM  MM     M8 MM    MM 8M
//     MM         MM    MM  MM.   ,M9 MM    MM YM.    ,
//   .JMML.       `Mbod"YML.P^YbmdP'.JMML..JMML.YMbmd'
//
//

void
page_table_init (struct hash *page_table)
{
    hash_init (page_table, page_hash_func, page_less_func, NULL);
}

void
page_table_destroy (struct hash *page_table)
{
    hash_destroy (page_table, page_action_func);
}

bool
page_load (struct SP_entry *page_entry)
{
    bool success = false;
    page_entry->pinned = true;
    if (page_entry->is_loaded) {
        return success;
    }
    switch (page_entry->type) {
        case SP_FILE:
            success = load_file (page_entry);
            break;
        case SP_SWAP:
            success = load_swap (page_entry);
            break;
        case SP_MMAP:
            success = load_file (page_entry);
            break;
        case SP_ERROR:
            PANIC ("SP type should not be ERROR");
    }
    return success;
}

bool
page_find (const void * vaddr) {
    return get_page_entry (vaddr) != NULL;
}

bool
page_find_and_load (const void * vaddr, const void * esp, const bool to_write)
{
    if (!is_user_vaddr (vaddr) || vaddr < USER_ADDRESS_BOTTOM) {
        return false;
    }

    struct SP_entry *page_entry = get_page_entry (vaddr);
    if (page_entry) {
        if (to_write && !page_entry->writable) {
            return false;
        }

        if (page_load (page_entry)) {
            page_entry->pinned = false;
            return true;
        } else {
            return false;
        }
    } else if (vaddr >= esp - STACK_HEURISTIC) {
        return grow_stack ((void *) vaddr);
    } else {
        return false;
    }
}

bool
page_add_file (struct file *file, int32_t ofs, uint8_t *upage,
               uint32_t read_bytes, uint32_t zero_bytes,
               bool writable)
{
    struct SP_entry *page_entry = malloc (sizeof (struct SP_entry));
    if (!page_entry) {
        return false;
    }
    page_entry->file = file;
    page_entry->offset = ofs;
    page_entry->page = upage;
    page_entry->read_bytes = read_bytes;
    page_entry->zero_bytes = zero_bytes;
    page_entry->writable = writable;
    page_entry->is_loaded = false;
    page_entry->type = SP_FILE;
    page_entry->pinned = false;

    return (hash_insert (&thread_current()->page_table, &page_entry->elem) == NULL);
}

bool
page_add_mmap(struct file *file, int32_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes)
{
    struct SP_entry *page_entry = malloc (sizeof (struct SP_entry));
    if (!page_entry) {
        return false;
    }
    page_entry->file = file;
    page_entry->offset = ofs;
    page_entry->page = upage;
    page_entry->read_bytes = read_bytes;
    page_entry->zero_bytes = zero_bytes;
    page_entry->is_loaded = false;
    page_entry->type = SP_MMAP;
    page_entry->writable = true;
    page_entry->pinned = false;

    if (!process_add_mmap (page_entry)) {
        free (page_entry);
        return false;
    }

    if (hash_insert (&thread_current()->page_table, &page_entry->elem)) {
        page_entry->type = SP_ERROR;
        return false;
    }

    return true;
}

bool
grow_stack (void *page)
{
    if ((size_t) (PHYS_BASE - pg_round_down (page)) > MAX_STACK_SIZE) {
        return false;
    }
    struct SP_entry *page_entry = malloc (sizeof (struct SP_entry));
    if (!page_entry) {
        return false;
    }
    page_entry->page = pg_round_down (page);
    page_entry->is_loaded = true;
    page_entry->writable = true;
    page_entry->type = SP_SWAP;
    page_entry->pinned = true;

    uint8_t *frame = frame_alloc (PAL_USER, page_entry);
    if (!frame) {
        free(page_entry);
        return false;
    }

    if (!install_page (page_entry->page, frame, page_entry->writable)) {
        free (page_entry);
        frame_free (frame);
        return false;
    }

    if (intr_context ()) {
        page_entry->pinned = false;
    }

    return (hash_insert (&thread_current()->page_table, &page_entry->elem) == NULL);
}
