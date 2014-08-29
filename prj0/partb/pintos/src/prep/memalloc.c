#include <stdio.h>
#include "memalloc.h"

#define FREE_BLOCK_SIZE                 (sizeof(struct free_block))
#define USED_BLOCK_SIZE                 (sizeof(struct used_block))

#define p_relative(PTR)                 ((void *)(PTR) - g_base)
#define p_offset(PTR, OFFSET)           ((void *)((void *)(PTR) + (size_t)(OFFSET)))
#define p_lt(P1, P2)                    ((void *)(P1) < (void *)(P2))
#define p_eq(P1, P2)                    ((void *)(P1) == (void *)(P2))

#define get_elem(FB_P)                  (&((FB_P)->elem))
#define get_data(UB_P)                  (&((UB_P)->data))
#define get_free_block(ELEM_P)          list_entry ((ELEM_P), struct free_block, elem)
#define get_used_block(DATA_P)          ((struct used_block *) p_offset ((DATA_P), -(USED_BLOCK_SIZE)))

#define block_begin()                   get_free_block (list_begin (&free_block_list))
#define block_end()                     get_free_block (list_end (&free_block_list))
#define block_rbegin()                  get_free_block (list_rbegin (&free_block_list))
#define block_rend()                    get_free_block (list_rend (&free_block_list))
#define block_head()                    get_free_block (list_head (&free_block_list))
#define block_tail()                    get_free_block (list_tail (&free_block_list))

#define block_next(FB_P)                get_free_block (list_next (get_elem (FB_P)))
#define block_prev(FB_P)                get_free_block (list_prev (get_elem (FB_P)))

#define block_insert(FBB_P, FB_P)       list_insert (get_elem (FBB_P), get_elem (FB_P))
#define block_append(FBB_P, FB_P)       block_insert (block_next (FBB_P), FB_P)
#define block_push_front(FB_P)          list_push_front (&free_block_list, get_elem (FB_P))
#define block_push_back(FB_P)           list_push_back (&free_block_list, get_elem (FB_P))
#define block_remove(FB_P)              list_remove (get_elem (FB_P));

#define block_adjacent(B1_P, B2_P)      (p_eq (p_offset ((B1_P), (B1_P)->length), B2_P))
#define block_union(FB_P) ({ \
    ASSERT ((FB_P) != NULL || !"Point NULL!"); \
    ASSERT ((FB_P) != block_rend () || !"Can't union head block!"); \
    ASSERT (block_next (FB_P) != NULL || !"Con't union end block!"); \
    ASSERT (block_next (FB_P) != block_end () || !"Can't union end block!"); \
    ASSERT (block_adjacent ((FB_P), block_next (FB_P)) || !"Blocks are not adjacent!"); \
    (FB_P)->length += (block_next (FB_P))->length; \
    block_remove (block_next (FB_P)); \
})

#define build_free_block(BASE, LENGTH)  ({ \
    ASSERT (((LENGTH) >= FREE_BLOCK_SIZE) || !"Not enough length for free_block."); \
    ((struct free_block *) (BASE))->length = (LENGTH); \
    ((struct free_block *) (BASE)); \
})
#define build_used_block(BASE, LENGTH)  ({ \
    ASSERT (((LENGTH) >= USED_BLOCK_SIZE) || !"Not enough length for used_block."); \
    ((struct used_block *) (BASE))->length = (LENGTH); \
    ((struct used_block *) (BASE)); \
})

struct list free_block_list;
void *g_base, *g_peak;


// Initialize memory allocator to use 'length' bytes of memory at 'base'.
void mem_init(uint8_t *base, size_t length)
{
    list_init (&free_block_list);
    g_base = base;
    g_peak = p_offset (base, length);
    block_push_front (build_free_block (base, length));
}

// Allocate 'length' bytes of memory.
void * mem_alloc(size_t length)
{
    if (length == 0) return NULL;

    size_t u_length = USED_BLOCK_SIZE + length;

    struct free_block *f;
    for (f = block_begin (); f != block_end (); f = block_next(f)) {
        if (f->length >= u_length) {
            if (f->length >= u_length + FREE_BLOCK_SIZE) {
                // create a new free block
                struct free_block *next = block_next(f);
                block_remove (f);
                block_insert (next, build_free_block (p_offset (f, u_length), f->length - u_length));
            } else {
                u_length = f->length;
                block_remove (f);
            }

            return get_data (build_used_block (f, u_length));
        }
    }

    return NULL;
}

// Free memory pointed to by 'ptr'.
void mem_free(void *ptr)
{
    struct used_block *u = get_used_block(ptr);

    struct free_block *f, *target = block_rend ();
    for (f = block_rbegin (); f != block_rend (); f = block_prev (f)) {
        if (p_lt (f, u)) { // | f |...| u |...| f->next |
            target = f;
            break;
        }
    }

    struct free_block *next = block_next (target);
    // target: interior or head
    // next: interior or tail

    // | target |...| u |...| next |
    if (u->length >= FREE_BLOCK_SIZE) { // space enough, create a new free_block
        struct free_block *new_block = build_free_block (u, u->length);
        block_insert (next, new_block);

        // if not tail and adjacent
        if ((next != block_end ()) && (block_adjacent (new_block, next))) {
            block_union(new_block);
        }

        // if not head and adjacent
        if ((target != block_rend ()) && (block_adjacent (target, new_block))) {
            block_union(target);
        }

        return;
    }

    // Not enough space for a new free_block

    // not head and | target | u |...| next |
    if ((target != block_rend ()) && (block_adjacent (target, u))) {
        target->length += u->length;
        return;
    }

    // not tail and | target |...| u | next |
    if ((next != block_end ()) && (block_adjacent (u, next))) {
        size_t new_length = u->length + next->length;
        block_remove (next);
        block_append (target, build_free_block (u, new_length));
        return;
    }

    // Now, no free_block on either side.

    // not the beginning, merge to prev block
    if (p_lt (g_base, u)) {
        struct used_block *to_merge;
        if (target == block_rend ()) {
            to_merge = (struct used_block *) g_base;
        } else {
            to_merge = (struct used_block *) p_offset (target, target->length);
        }
        // search for the most adjacent one
        while (p_lt (p_offset (to_merge, to_merge->length), u)) {
            to_merge = p_offset (to_merge, to_merge->length);
        }

        to_merge->length += u->length;
        return;
    }

    // at the beginning, merge to next used block
    ASSERT(false && "It's impossible to free a small block at the beginning!");
    // char *p = 0; *p = 0;
}

// Return the number of elements in the free list.
size_t mem_sizeof_free_list(void)
{
    return list_size (&free_block_list);
}

// Dump the free list.
#define DUMP_BLOCK(PTR, NAME) printf(#NAME ":\t%p -> %p, %zu\n", (PTR), p_offset ((PTR), (PTR)->length), (PTR)->length);
void mem_dump_free_list(void)
{
    DUMP_BLOCK (block_rend (), head);
    struct free_block *f;
    for (f = block_begin (); f != block_end (); f = block_next (f)) {
        DUMP_BLOCK (f, block);
    }
    DUMP_BLOCK (block_end (), tail);
    printf("=====Dumped=====\n");
}
#undef DUMP_BLOCK
