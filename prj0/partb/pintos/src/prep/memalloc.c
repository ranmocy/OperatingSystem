#include <stdio.h>
#include "memalloc.h"

#undef ASSERT
#define ASSERT(CONDITION, MSG)      if (CONDITION) { } else { PANIC("assertion `%s' failed.", MSG); }
#define ASSERT_PTR(PTR, MSG)        ASSERT((void *)(g_base) <= (void *)(PTR), "PTR: " MSG " underflow!"); \
                                    ASSERT((void *)(PTR) < (void *)(g_bound), "PTR: " MSG " overflow!")
#define ASSERT_BLOCK(FB_P, MSG)     ASSERT_PTR(FB_P, MSG); ASSERT_PTR((void *)FB_P + FB_P->length, MSG)

#define addr_d(PTR)                 (int)((void *)PTR - (void *)g_base)
#define block_tail_p(FB_P)          (void *)FB_P + FB_P->length

#define get_free_block(ELEM_P)      list_entry(ELEM_P, struct free_block, elem)
#define get_used_block(PTR)         (struct used_block *)(PTR - sizeof(struct used_block))
#define block_begin()               get_free_block(list_begin(&free_block_list))
#define block_end()                 get_free_block(list_end(&free_block_list))
#define block_rend()                get_free_block(list_rend(&free_block_list))
#define block_next(FB_P)            get_free_block(list_next(&(FB_P->elem)))
#define block_prev(FB_P)            get_free_block(list_prev(&(FB_P->elem)))
#define block_insert(FB1_P, FB2_P)  list_insert(&(FB1_P->elem), &(FB2_P->elem))
#define block_append(FB1_P, FB2_P)  block_insert(block_next(FB1_P), FB2_P)
#define block_push_front(FB_P)      list_push_front(&free_block_list, &(FB_P->elem))
#define block_remove(FB_P)          list_remove(&(FB_P->elem))


struct list free_block_list;
uint8_t *g_base, *g_bound;


// Union free_block CURRENT with the next free_block if they are adjacent
void block_union(struct free_block *current)
{
    ASSERT_BLOCK(current, "block_union: current");

    struct free_block *next = block_next(current); \
    if (block_tail_p(current) == (void *)next) { \
        ASSERT_BLOCK(next, "block_union: next");

        struct free_block *next = block_next(current);
        current->length += next->length;
        list_remove(&(next->elem));
    }
}


// Initialize memory allocator to use LENGTH bytes of memory at BASE
void mem_init(uint8_t *base, size_t length)
{
    g_base = base;
    g_bound = base + length;
    list_init(&free_block_list);

    struct free_block *f = (struct free_block *) base;
    f->length = length;
    block_push_front(f);
}


// Allocate LENGTH bytes of memory
void * mem_alloc(size_t length)
{
    if (length == 0) return NULL;

    size_t length_needed = sizeof(struct used_block) + length;

    struct free_block *f;
    for (f = block_begin(); f != block_end(); f = block_next(f)) {
        if (f->length >= length_needed) { // we have enough space
            struct used_block *u = (struct used_block *) f;
            struct free_block *next = block_next(f);
            size_t f_length = f->length;
            block_remove(f);

            // if we don't have enough for another free_block at the end
            if (f_length < length_needed + sizeof(struct free_block)) {
                // then we use the entire f
                u->length = f_length;
            } else {
                // then we create and insert another free_block
                u->length = length_needed;

                struct free_block *n = (struct free_block *)((void *)f + length_needed);
                n->length = f_length - length_needed;
                block_insert(next, n);
            }

            return &(u->data);
        }
    }

    return NULL;
}

// Free memory pointed to by PTR
void mem_free(void *ptr)
{
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
    block_union(f);
    block_union(block_prev(f));
}

// Return the number of elements in the free list.
size_t mem_sizeof_free_list()
{
    return list_size(&free_block_list);
}

// Dump the free list.  Implementation of this method is optional.
#define DUMP_BLOCK(FB_P, NAME) printf("\t" NAME ":\t%d -> %d: length %d\n", addr_d(FB_P), addr_d(FB_P + FB_P->length), (int)(FB_P->length));
void mem_dump_free_list()
{
    DUMP_BLOCK(block_rend(), "head");
    for (struct free_block *f = block_begin(); f != block_end(); f = block_next(f))
        DUMP_BLOCK(f, "block");
    DUMP_BLOCK(block_end(), "tail");
}
#undef DUMP_BLOCK
