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

#define p_addr(PTR) (int)((void *)PTR - (void *)g_base)
#define debug(STR, ...) printf(KYEL "\nDEBUG: " STR "\n" RESET, __VA_ARGS__)

#define get_free_block(ELEM_P)      list_entry(ELEM_P, struct free_block, elem)
#define get_used_block(PTR)         (struct used_block *)(PTR - sizeof(struct used_block))
#define block_begin()               get_free_block(list_begin(&free_block_list))
#define block_end()                 get_free_block(list_end(&free_block_list))
#define block_next(FB_P)            get_free_block(list_next(&(FB_P->elem)))
#define block_prev(FB_P)            get_free_block(list_prev(&(FB_P->elem)))
#define block_insert(FB1_P, FB2_P)  list_insert(&(FB1_P->elem), &(FB2_P->elem))
#define block_append(FB1_P, FB2_P)  block_insert(block_next(FB1_P), FB2_P)
#define block_push_front(FB_P)      list_push_front(&free_block_list, &(FB_P->elem))
#define block_remove(FB_P)          list_remove(&(FB_P->elem))


struct list free_block_list;
uint8_t *g_base;


// splice block F into two block with length LENGTH, and F->length - LENGTH
void block_slice(struct free_block *f, size_t length)
{
    struct free_block *n = (struct free_block *)((void *)f + length);
    n->length = f->length - length;
    f->length = length;
    block_append(f, n);
}

// Union two free_block CURRENT with the next free_block
void union_block(struct free_block *current)
{
    struct free_block *next = block_next(current);
    debug("UNION_BLOCK: %d with %d", p_addr(current), p_addr(next));
    current->length += next->length;
    list_remove(&(next->elem));
}

// Union two free_block CURRENT with the next free_block if they are adjacent
// void union_block_if_adjacent(struct free_block *current)
void union_block_if_adjacent(struct free_block *current)
{
    struct free_block *next = block_next(current); \
    if ((void *)current + current->length == (void *)next) { \
        union_block(current);
    }
}


// Initialize memory allocator to use LENGTH bytes of memory at BASE
void mem_init(uint8_t *base, size_t length)
{
    g_base = base; // DEBUG:
    debug("BASE: %d -> %d, LENGTH: %d", (int)base, (int)(base + length), (int)length);

    list_init(&free_block_list);

    struct free_block *f = (struct free_block *) base;
    f->length = length;
    block_push_front(f);

    mem_dump_free_list(); // DEBUG:
}


// Allocate LENGTH bytes of memory
void * mem_alloc(size_t length)
{
    if (length == 0) return NULL;

    size_t length_needed = sizeof(struct used_block) + length;
    debug(KRED "MALLOC: %d, real: %d", (int)length, (int)length_needed);

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
    debug(KGRN "FREE: %d", p_addr(ptr));
    struct used_block *u = get_used_block(ptr);
    struct free_block *next_free_block = block_end(); // Default
    struct free_block *current;
    for (current = block_begin(); current != block_end(); current = block_next(current)) {
        if ((void *)u < (void *)current) {
            next_free_block = current;
            break;
        }
    }

    struct free_block *f = (struct free_block *) u;
    f->length = u->length;
    block_insert(next_free_block, f);

    // union if adjacent
    union_block_if_adjacent(f);
    union_block_if_adjacent(block_prev(f));

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
