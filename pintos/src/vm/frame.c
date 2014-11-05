#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/frame.h"

struct list frame_table;
struct lock frame_table_lock;


void
frame_add (void *frame)
{
    // TODO
}

void *
frame_evict (void)
{
    // TODO
    return NULL;
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
frame_alloc (enum palloc_flags flags, struct page_entry *page_entry)
{
    // TODO
    return NULL;
}

void
frame_free (void *frame)
{
    // TODO
}
