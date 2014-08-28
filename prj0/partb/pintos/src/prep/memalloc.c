#include <stdio.h>
#include "memalloc.h"

#define p_addr(PTR) (int)((uint8_t *)PTR - g_base)


struct list free_block_list;
uint8_t *g_base;


/* Initialize memory allocator to use 'length'
   bytes of memory at 'base'. */
void mem_init(uint8_t *base, size_t length)
{
    g_base = base;
    printf("\nDEBUG: BASE: %d -> %d, LENGTH: %d\n", (int)base, (int)(base + length), (int)length);
    // size_t *test = (size_t *)(base + length - sizeof(size_t));
    // *test = 1;

    list_init(&free_block_list);

    struct free_block *block_p = (struct free_block *) base;
    block_p->length = length;
    list_push_front(&free_block_list, &(block_p->elem));
    printf("DEBUG: list_size: %d\n", (int)list_size(&free_block_list));
}

/* Allocate 'length' bytes of memory. */
void * mem_alloc(size_t length)
{
    printf("\nDEBUG: MALLOC: %d\n", (int)length);

    size_t length_needed = sizeof(struct used_block) + length;
    printf("DEBUG: search: %d blocks for length %d\n", (int)list_size(&free_block_list), (int)length_needed);

    for (struct list_elem *e = list_begin(&free_block_list);
         e != list_end(&free_block_list);
         e = list_next(e))
    {
        struct free_block *f = list_entry(e, struct free_block, elem);
        printf("  DEBUG: free_block: %d\n", p_addr(f));

        // if we have more space
        if (f->length > length_needed + sizeof(struct free_block)) {
            struct free_block *n = (struct free_block *)((void *)f + length_needed);
            printf("  DEBUG: %d + %d = %d\n", p_addr(f), (int)length_needed, p_addr(n));
            printf("%d\n", (int)sizeof(struct free_block));
            printf("  DEBUG: before_insert: %d, %d\n", p_addr(list_next(e)), p_addr(&(n->elem)));
            n->length = f->length - length_needed;
            printf("  DEBUG: next_block: %d, length: %d\n", p_addr(n), (int)(n->length));
            printf("  DEBUG: before_insert: %d, %d\n", p_addr(list_next(e)), p_addr(&(n->elem)));

                // mem_dump_free_list(); // DEBUG:
            list_insert(list_next(e), &(n->elem));
        }

        // or if we can use the whole block
        if (f->length >= length_needed) {
            mem_dump_free_list(); // DEBUG:
            printf("  DEBUG: removing: &%d -> (&%d) -> &%d\n", p_addr(list_prev(e)), p_addr(e), p_addr(list_next(e)));
            list_remove(e);
            mem_dump_free_list(); // DEBUG:

            struct used_block *u = (struct used_block *) f;
            u->length = length;

            printf("  DEBUG: ptr: %d\n", (int)&(u->data));
            return &(u->data);
        }
    }

    return NULL;
}

/* Free memory pointed to by 'ptr'. */
void mem_free(void *ptr)
{
    printf("\nDEBUG: FREE: %d\n", p_addr(ptr));
    mem_dump_free_list(); // DEBUG:

    struct used_block *u = (struct used_block *)(ptr - sizeof(struct used_block));
    printf("DEBUG: %d - %d = %d\n", p_addr(ptr), (int)sizeof(struct used_block), p_addr(u));
    size_t block_length = u->length + sizeof(struct used_block);
    printf("DEBUG: block from %d, length %d\n", p_addr(u), (int)block_length);

    for (struct list_elem *e = list_begin (&free_block_list);
         e != list_end (&free_block_list);
         e = list_next (e))
    {
        printf("DEBUG: block: %d\n", p_addr(&e));
        // TODO:
    }

    struct free_block *f = (struct free_block *) u;
    f->length = block_length;
    printf("DEBUG: free_block: %d, length %d\n", p_addr(f), (int)(f->length));
    list_push_front(&free_block_list, &(f->elem));
    mem_dump_free_list(); // DEBUG:
}

/* Return the number of elements in the free list. */
size_t mem_sizeof_free_list(void)
{
    printf("\nDEBUG: LIST_SIZE: %d\n", (int)list_size(&free_block_list));
    return list_size(&free_block_list);
}

/* Dump the free list.  Implementation of this method is optional. */
void mem_dump_free_list(void)
{
    printf("Dumping the free list:\n");
    struct free_block *f;
    f = list_entry(list_rend(&free_block_list), struct free_block, elem);
    printf("\thead:\t%d -> %d: length %d\n", p_addr(f), p_addr(f + f->length), (int)(f->length));
    for (struct list_elem *e = list_begin(&free_block_list);
         e != list_end(&free_block_list);
         e = list_next(e))
    {
        f = list_entry(e, struct free_block, elem);
        printf("\tblock:\t%d -> %d: length %d\n", p_addr(f), p_addr(f + f->length), (int)(f->length));
    }
    f = list_entry(list_end(&free_block_list), struct free_block, elem);
    printf("\ttail:\t%d -> %d: length %d\n", p_addr(f), p_addr(f + f->length), (int)(f->length));
    printf("Dumped.\n");
}
