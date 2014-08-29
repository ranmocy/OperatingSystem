#include <stdio.h>
#include "memalloc.h"


#undef ASSERT
#define ASSERT(CONDITION, MSG)      if (CONDITION) { } else { PANIC("assertion `" MSG "' failed."); }
#define ASSERT_PTR(PTR, MSG)        ASSERT(((void *)(g_base) <=  (void *)(PTR)),      "PTR: " MSG " underflow!"); \
                                    ASSERT(((void *)(PTR)    <   (void *)(g_peak)),   "PTR: " MSG " overflow!")
#define ASSERT_BYTE(FB_P)           *(char *)(FB_P) = 0;
#define ASSERT_BLOCK(FB_P, MSG)     ASSERT_PTR(FB_P, MSG); ASSERT_PTR((void *)FB_P + FB_P->length, MSG)

#define addr_d(PTR)                 (void *)((void *)(PTR) - (void *)g_base)
#define addr_offset(PTR, offset)    ((void *)(PTR) + (size_t)(offset))
#define block_head_p(FB_P)          ((void *)(FB_P))
#define block_tail_p(FB_P)          addr_offset(block_head_p(FB_P), FB_P->length)
#define elem_p(FB_P)                &(FB_P->elem)

#define get_free_block(ELEM_P)      list_entry(ELEM_P, struct free_block, elem)
#define get_used_block(PTR)         (struct used_block *)((void *)PTR - sizeof(struct used_block))
#define block_begin()               get_free_block  (list_begin     (&free_block_list))
#define block_end()                 get_free_block  (list_end       (&free_block_list))
#define block_rend()                get_free_block  (list_rend      (&free_block_list))
#define block_next(FB_P)            get_free_block  (list_next      (elem_p(FB_P)))
#define block_prev(FB_P)            get_free_block  (list_prev      (elem_p(FB_P)))
#define block_insert(FB1_P, FB2_P)  list_insert     (elem_p(FB1_P),             elem_p(FB2_P))
#define block_append(FB1_P, FB2_P)  list_insert     (list_next(elem_p(FB1_P)),  elem_p(FB2_P))
#define block_push_front(FB_P)      list_push_front (&free_block_list,          elem_p(FB_P))
#define block_remove(FB_P)          list_remove     (elem_p(FB_P))

struct free_block * build_free_block(void *base, size_t length)
{
    struct free_block *f = (struct free_block *) base;
    f->length = length;
    return f;
}

struct used_block * build_used_block(void *base, size_t length)
{
    struct used_block *u = (struct used_block *) base;
    u->length = length;
    return u;
}


struct list free_block_list;
uint8_t *g_base, *g_peak;


// Union free_block CURRENT with the next free_block if they are adjacent
void block_union(struct free_block *current)
{
    ASSERT_BLOCK(current, "block_union: current");

    struct free_block *next = block_next(current); \
    if (block_tail_p(current) == block_head_p(next)) { \
        ASSERT_BLOCK(next, "block_union: next");

        current->length += next->length;
        block_remove(next);
    }
}


// Initialize memory allocator to use LENGTH bytes of memory at BASE
void mem_init(uint8_t *base, size_t length)
{
    g_base = base;
    g_peak = base + length;
    list_init(&free_block_list);

    ASSERT_BYTE(base);
    ASSERT_BYTE((void *)base + length);

    block_push_front(build_free_block(base, length));
}


// Allocate LENGTH bytes of memory
void * mem_alloc(size_t length)
{
    if (length == 0) return NULL;

    printf("Mem_alloc: %zu\n", length);
    mem_dump_free_list();

    size_t u_length = sizeof(struct used_block) + length;

    struct free_block *f;
    for (f = block_begin(); f != block_end(); f = block_next(f)) {
        if (f->length >= u_length) { // we have enough space
            ASSERT_BLOCK(f, "mem_alloc: loop: begin");

            struct used_block *u = (struct used_block *) f;
            struct free_block *next = block_next(f);

            if (next != block_end()) {
                ASSERT_BLOCK(next, "mem_alloc: loop: next");
            }

            size_t f_length = f->length;
            block_remove(f);

            // if we don't have enough for another free_block at the end
            if (f_length < u_length + sizeof(struct free_block)) {
                // then we use the entire f
                u->length = f_length;
            } else {
                // then we create and insert another free_block
                u->length = u_length;

                struct free_block *n = (struct free_block *)((void *)f + u_length);
                n->length = f_length - u_length;
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
#define DUMP_BLOCK(FB_P, NAME) printf("\t" NAME ":\t%p -> %p: length %zu\n", addr_d(FB_P), addr_d((void *)FB_P + FB_P->length), (FB_P->length));
void mem_dump_free_list()
{
    DUMP_BLOCK(block_rend(), "head");
    for (struct free_block *f = block_begin(); f != block_end(); f = block_next(f))
        DUMP_BLOCK(f, "block");
    DUMP_BLOCK(block_end(), "tail");
}
#undef DUMP_BLOCK
