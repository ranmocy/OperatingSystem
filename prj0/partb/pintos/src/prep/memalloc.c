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

#define p_addr(PTR) (int)((uint8_t *)PTR - g_base)
#define get_free_block(ELEM_P) list_entry(ELEM_P, struct free_block, elem)
#define next_free_block(FB_P) get_free_block(list_next(&(FB_P->elem)))
#define block_insert(FB1_P, FB2_P) list_insert(&(FB1_P->elem), &(FB2_P->elem))
#define block_next(FB_P) get_free_block(list_next(&(FB_P->elem)))
#define block_prev(FB_P) get_free_block(list_prev(&(FB_P->elem)))


struct list free_block_list;
uint8_t *g_base;


/* Initialize memory allocator to use 'length'
   bytes of memory at 'base'. */
void mem_init(uint8_t *base, size_t length)
{
    g_base = base; // DEBUG:
    printf("\nDEBUG: BASE: %d -> %d, LENGTH: %d\n", (int)base, (int)(base + length), (int)length);
    // printf("DEBUG: list: %d, head: %d, tail: %d\n", p_addr(&free_block_list), p_addr(&(free_block_list.head)), p_addr(&(free_block_list.tail)));
    // printf("DEBUG: list: size: %d\n", (int)sizeof(struct list));

    list_init(&free_block_list);

    struct free_block *block_p = (struct free_block *) base;
    block_p->length = length;
    list_push_front(&free_block_list, &(block_p->elem));
    mem_dump_free_list();
}

/* Allocate 'length' bytes of memory. */
void * mem_alloc(size_t length)
{
    printf(KRED "\nDEBUG: MALLOC: %d\n" RESET, (int)length);
    mem_dump_free_list(); // DEBUG:

    size_t length_needed = sizeof(struct used_block) + length;
    // printf("DEBUG: search: %d blocks for length %d\n", (int)list_size(&free_block_list), (int)length_needed);

    for (struct list_elem *e = list_begin(&free_block_list);
         e != list_end(&free_block_list);
         e = list_next(e))
    {
        struct free_block *f = list_entry(e, struct free_block, elem);
        // printf("  DEBUG: free_block: %d\n", p_addr(f));

        // if we have more space
        if (f->length > length_needed + sizeof(struct free_block)) {
            struct free_block *n = (struct free_block *)((void *)f + length_needed);
            // printf("  DEBUG: %d + %d = %d\n", p_addr(f), (int)length_needed, p_addr(n));
            n->length = f->length - length_needed;
            // printf("  DEBUG: next_block: %d, length: %d\n", p_addr(n), (int)(n->length));
            printf("  DEBUG: before_insert: %d, %d\n", p_addr(list_next(e)), p_addr(&(n->elem)));

                // mem_dump_free_list(); // DEBUG:
            list_insert(list_next(e), &(n->elem));
        }

        // or if we can use the whole block
        if (f->length >= length_needed) {
                // mem_dump_free_list(); // DEBUG:
            // printf("  DEBUG: removing: &%d -> (&%d) -> &%d\n", p_addr(list_prev(e)), p_addr(e), p_addr(list_next(e)));
            list_remove(e);
                // mem_dump_free_list(); // DEBUG:

            struct used_block *u = (struct used_block *) f;
            u->length = length;

            // printf("  DEBUG: ptr: %d\n", (int)&(u->data));
            mem_dump_free_list(); // DEBUG:

            return &(u->data);
        }
    }

    printf("DEBUG: MALLOC FAILED!\n");
    return NULL;
}

// Union two free_block CURRENT with the next free_block
void union_block(struct free_block *current) {
    struct free_block *next = next_free_block(current);
    printf("\nDEBUG: UNION_BLOCK: %d with %d\n", p_addr(current), p_addr(next));
    current->length += next->length;
    list_remove(&(next->elem));
}

/* Free memory pointed to by 'ptr'. */
void mem_free(void *ptr)
{
    printf(KGRN "\nDEBUG: FREE: %d\n" RESET, p_addr(ptr));
    mem_dump_free_list(); // DEBUG:

    struct used_block *u = (struct used_block *)(ptr - sizeof(struct used_block));
    // printf("DEBUG: %d - %d = %d\n", p_addr(ptr), (int)sizeof(struct used_block), p_addr(u));
    size_t block_length = u->length + sizeof(struct used_block);
    // printf("DEBUG: block from %d, length %d\n", p_addr(u), (int)block_length);

    struct free_block *next_free_block = get_free_block(list_end(&free_block_list));

    for (struct list_elem *e = list_begin(&free_block_list);
         e != list_end(&free_block_list);
         e = list_next(e))
    {
        struct free_block *current = list_entry(e, struct free_block, elem);
        // printf("  DEBUG: block: (%d) <=> %d\n", p_addr(u), p_addr(current));

        if ((uintptr_t)u < (uintptr_t)current) {
            next_free_block = current;
            break;
        }
    }
    // printf("DEBUG: got_block: %d\n", p_addr(next_free_block));

    struct free_block *f = (struct free_block *) u;
    f->length = block_length;
    // printf("DEBUG: free_block: %d, length %d\n", p_addr(f), (int)(f->length));
    block_insert(next_free_block, f);
        // mem_dump_free_list(); // DEBUG:

    // if we need to union before
    struct free_block *prev = block_prev(f);
    // printf("DEBUG: union_prev: %d + %d == %d\n", p_addr(prev), (int)(prev->length), p_addr(f));
    if ((uintptr_t)prev + prev->length == (uintptr_t)f) {
        union_block(prev);
        f = prev;
    }

    // if we need to union after
    struct free_block *next = block_next(f);
    // printf("DEBUG: union_next: %d + %d == %d\n", p_addr(f), (int)(f->length), p_addr(next));
    if ((uintptr_t)f + f->length == (uintptr_t)next) {
        // printf("DEBUG: \n");
        union_block(f);
    }

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
    // printf("Dumping the free list:\n");
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
    // printf("Dumped.\n");
}
