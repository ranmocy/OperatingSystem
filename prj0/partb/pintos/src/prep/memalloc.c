#include <stdio.h>
#include "memalloc.h"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define RESET "\033[0m"

#define p_addr(PTR) (int)((uintptr_t)PTR - (uintptr_t)g_base)

#define get_free_block(ELEM_P) list_entry(ELEM_P, struct free_block, elem)
#define block_begin() get_free_block(list_begin(&free_block_list))
#define block_end() get_free_block(list_end(&free_block_list))
#define block_next(FB_P) get_free_block(list_next(&(FB_P->elem)))
#define block_prev(FB_P) get_free_block(list_prev(&(FB_P->elem)))
#define block_insert(FB1_P, FB2_P) list_insert(&(FB1_P->elem), &(FB2_P->elem))
#define block_append(FB1_P, FB2_P) block_insert(block_next(FB1_P), FB2_P)
#define block_push_front(FB_P) list_push_front(&free_block_list, &(FB_P->elem))
#define block_remove(FB_P) list_remove(&(FB_P->elem))

struct list free_block_list;
uint8_t *g_base;


// splice block F into two block with length LENGTH, and F->length - LENGTH
void block_slice(struct free_block *f, size_t length)
{
    struct free_block *n = (struct free_block *)((uintptr_t)f + length);
    n->length = f->length - length;
    f->length = length;
    block_append(f, n);
}

// Union two free_block CURRENT with the next free_block
void union_block(struct free_block *current)
{
    struct free_block *next = block_next(current);
    printf(KYEL "\nDEBUG: UNION_BLOCK: %d with %d\n" RESET, p_addr(current), p_addr(next));
    current->length += next->length;
    list_remove(&(next->elem));
}



// Initialize memory allocator to use LENGTH bytes of memory at BASE
void mem_init(uint8_t *base, size_t length)
{
    g_base = base; // DEBUG:
    printf("\nDEBUG: BASE: %d -> %d, LENGTH: %d\n", (int)base, (int)(base + length), (int)length);

    list_init(&free_block_list);

    struct free_block *f = (struct free_block *) base;
    f->length = length;
    block_push_front(f);

    mem_dump_free_list(); // DEBUG:
}


// Allocate LENGTH bytes of memory
void * mem_alloc(size_t length)
{
    size_t length_needed = sizeof(struct used_block) + length;
    printf(KRED "\nDEBUG: MALLOC: %d, real: %d\n" RESET, (int)length, (int)length_needed);
    mem_dump_free_list(); // DEBUG:

    struct free_block *f;
    for (f = block_begin(); f != block_end(); f = block_next(f)) {
        // if we have space to slice the block into two
        if (f->length > length_needed + sizeof(struct free_block)) {
            block_slice(f, length_needed);
        }

        // then if we can use the whole block
        if (f->length >= length_needed) {
            struct used_block *u = (struct used_block *) f;
            u->length = f->length;

            block_remove(f);
            mem_dump_free_list(); // DEBUG:
            return &(u->data);
        }
    }

    return NULL;
}

// Free memory pointed to by PTR
void mem_free(void *ptr)
{
    printf(KGRN "\nDEBUG: FREE: %d\n" RESET, p_addr(ptr));
    mem_dump_free_list(); // DEBUG:

    struct used_block *u = (struct used_block *)(ptr - sizeof(struct used_block));
    size_t block_length = u->length + sizeof(struct used_block);

    struct free_block *next_free_block = get_free_block(list_end(&free_block_list));

    for (struct list_elem *e = list_begin(&free_block_list);
         e != list_end(&free_block_list);
         e = list_next(e))
    {
        struct free_block *current = list_entry(e, struct free_block, elem);

        if ((uintptr_t)u < (uintptr_t)current) {
            next_free_block = current;
            break;
        }
    }

    struct free_block *f = (struct free_block *) u;
    f->length = block_length;
    block_insert(next_free_block, f);

    // if we need to union before
    struct free_block *prev = block_prev(f);
    if ((uintptr_t)prev + prev->length == (uintptr_t)f) {
        union_block(prev);
        f = prev;
    }

    // if we need to union after
    struct free_block *next = block_next(f);
    if ((uintptr_t)f + f->length == (uintptr_t)next) {
        union_block(f);
    }

    mem_dump_free_list(); // DEBUG:
}

// Return the number of elements in the free list.
size_t mem_sizeof_free_list()
{
    return list_size(&free_block_list);
}

// Dump the free list.  Implementation of this method is optional.
void mem_dump_free_list()
{
    struct free_block *f;
    f = list_entry(list_rend(&free_block_list), struct free_block, elem);
    printf("\thead:\t%d -> %d: length %d\n", p_addr(f), p_addr(f + f->length), (int)(f->length));
    for (struct list_elem *e = list_begin(&free_block_list);
         e != list_end(&free_block_list);
         e = list_next(e))
    {
        f = list_entry(e, struct free_block, elem);
        printf("\tblock:\t%d -> %d: length %d\n", p_addr(f), p_addr(f + f->length - 1), (int)(f->length));
    }
    f = list_entry(list_end(&free_block_list), struct free_block, elem);
    printf("\ttail:\t%d -> %d: length %d\n\n", p_addr(f), p_addr(f + f->length), (int)(f->length));
}
