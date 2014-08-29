#include <stdio.h>
#include "memalloc.h"

#define FREE_BLOCK_SIZE                 (sizeof(struct free_block))
#define USED_BLOCK_SIZE                 (sizeof(struct used_block))

#define p_relative(PTR)                 ((void *)(PTR) - g_base)
#define p_offset(PTR, OFFSET)           ((void *)((void *)(PTR) + (size_t)(OFFSET)))

#define get_elem(FB_P)                  (&((FB_P)->elem))
#define get_data(UB_P)                  (&((UB_P)->data))
#define get_free_block(ELEM_P)          list_entry ((ELEM_P), struct free_block, elem)
#define get_used_block(DATA_P)          ((struct used_block *) p_offset ((DATA_P), -(USED_BLOCK_SIZE)))

#define block_begin()                   get_free_block (list_begin (&free_block_list))
#define block_rbegin()                  get_free_block (list_rbegin (&free_block_list))
#define block_end()                     get_free_block (list_end (&free_block_list))
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

#define build_free_block(BASE, LENGTH)  ({ \
    ((struct free_block *) (BASE))->length = (LENGTH); \
    ((struct free_block *) (BASE)); \
})
#define build_used_block(BASE, LENGTH)  ({ \
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

}

// Return the number of elements in the free list.
size_t mem_sizeof_free_list(void)
{
    return list_size(&free_block_list);
}

// Dump the free list.
void mem_dump_free_list(void)
{

}
