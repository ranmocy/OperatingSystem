#include <stdio.h>
#include "memalloc.h"

#define MAX(A, B)                       ((A) > (B) ? (A) : (B))
#define FREE_BLOCK_SIZE                 (sizeof(struct free_block))
#define USED_BLOCK_SIZE                 (sizeof(struct used_block))
#define BLOCK_SIZE                      (MAX (FREE_BLOCK_SIZE, USED_BLOCK_SIZE))

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


// Initialize memory allocator to use 'length' bytes of memory at 'base'.
void mem_init(uint8_t *base, size_t length)
{
    list_init (&free_block_list);
    block_push_front (build_free_block (base, length));
}

// Allocate 'length' bytes of memory.
void * mem_alloc(size_t length)
{
    if (length == 0) return NULL;

    size_t u_length = MAX(BLOCK_SIZE, USED_BLOCK_SIZE + length);

    void *result = NULL;
    struct free_block *f;
    for (f = block_rbegin (); f != block_rend (); f = block_prev (f)) {
        if (f->length >= u_length) {
            if (f->length >= u_length + FREE_BLOCK_SIZE) {
                // shrink the size of the free_block
                f->length -= u_length;
                result = get_data (build_used_block (p_offset (f, f->length), u_length));
                break;
            } else {
                // use the entire free_block
                u_length = f->length;
                block_remove (f);
                result = get_data (build_used_block (f, u_length));
                break;
            }
        }
    }

    return result;
}

// Free memory pointed to by 'ptr'.
void mem_free(void *ptr)
{
    struct used_block *u = get_used_block(ptr);
    ASSERT((u->length >= FREE_BLOCK_SIZE) || !!!"Not enough space to create a new free_block!");

    struct free_block *f, *target = block_rend ();
    for (f = block_rbegin (); f != block_rend (); f = block_prev (f)) {
        if (p_lt (f, u)) { // | f |...| u |...| f->next |
            target = f;
            break;
        }
    }

    // | target |...| u |...| next |
    struct free_block *next = block_next (target);
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
