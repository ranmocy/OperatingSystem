#include "vm/swap.h"

#define SWAP_FREE 0
#define SWAP_IN_USE 1
#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct lock swap_lock;
struct block *swap_block;
struct bitmap *swap_map;

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
swap_init (void)
{
    swap_block = block_get_role (BLOCK_SWAP);
    ASSERT (swap_block != NULL && "BLOCK_SWAP is needed for swap!");

    swap_map = bitmap_create (block_size (swap_block) / SECTORS_PER_PAGE );
    ASSERT (swap_map != NULL && "swap_map is needed for swap!");

    bitmap_set_all (swap_map, SWAP_FREE);
    lock_init (&swap_lock);
}

void
swap_in (size_t used_index, void* frame)
{
    if (!swap_block || !swap_map) {
        return;
    }
    lock_acquire (&swap_lock);
    if (bitmap_test (swap_map, used_index) == SWAP_FREE) {
        PANIC ("Free swap can not be swap again.");
    }
    bitmap_flip (swap_map, used_index);

    size_t i;
    for (i = 0; i < SECTORS_PER_PAGE; i++) {
        block_read (swap_block, used_index * SECTORS_PER_PAGE + i,
                    (uint8_t *) frame + i * BLOCK_SECTOR_SIZE);
    }
    lock_release (&swap_lock);
}

size_t
swap_out (void *frame)
{
    if (!swap_block || !swap_map) {
        PANIC ("No swap partition is available.");
    }
    lock_acquire (&swap_lock);
    size_t free_index = bitmap_scan_and_flip (swap_map, 0, 1, SWAP_FREE);

    if (free_index == BITMAP_ERROR) {
        PANIC ("Swap partition is full.");
    }

    size_t i;
    for (i = 0; i < SECTORS_PER_PAGE; i++) {
        block_write (swap_block, free_index * SECTORS_PER_PAGE + i,
                     (uint8_t *) frame + i * BLOCK_SECTOR_SIZE);
    }
    lock_release (&swap_lock);
    return free_index;
}

