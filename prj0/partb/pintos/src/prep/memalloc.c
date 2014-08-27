#include <stdio.h>
#include "memalloc.h"

// #define list_header_length(LIST_ELEM, STRUCT, MEMBER)           \
//         ((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
//                      - offsetof (STRUCT, MEMBER.next)))


struct list free_block_list;


/* Initialize memory allocator to use 'length'
   bytes of memory at 'base'. */
void mem_init(uint8_t *base, size_t length)
{
    printf("DEBUG: BASE: %d, LENGTH: %d\n", (int)base, (int)length);

    list_init(&free_block_list);

    struct free_block *block_p = (struct free_block *) base;
    block_p->length = length;
    list_push_front(&free_block_list, &(block_p->elem));
    printf("DEBUG: list_size: %d\n", (int)list_size(&free_block_list));
}

/* Allocate 'length' bytes of memory. */
void * mem_alloc(size_t length)
{
    mem_dump_free_list();
    printf("DEBUG: MALLOC: %d\n", (int)length);

    for (struct list_elem *e = list_begin (&free_block_list);
         e != list_end (&free_block_list);
         e = list_next (e))
    {
        struct free_block *f = list_entry(e, struct free_block, elem);
        size_t length_needed = sizeof(struct used_block) + length;

        printf("DEBUG: search: %d blocks for length %d\n", (int)list_size(&free_block_list), (int)length_needed);

        if (f->length > length_needed) {
            // then we need to splice the block into two
            struct free_block *n = f + length_needed;
            n->length = length;
            list_insert(list_next(e), &(n->elem));
            printf("DEBUG: first: %d\n", (int)list_size(&free_block_list));
        }

        if (f->length >= length_needed) {
            list_remove(e);
            printf("DEBUG: list_size: %d\n", (int)list_size(&free_block_list));

            struct used_block *u = (struct used_block *) f;
            u->length = length;

            printf("DEBUG: ptr: %d\n", (int)&(u->data));
            return &(u->data);
        }
    }

    return NULL;
}

/* Free memory pointed to by 'ptr'. */
void mem_free(void *ptr)
{
    printf("DEBUG: FREE: %d\n", (int)ptr);
    struct used_block *u = (struct used_block *)(ptr - sizeof(struct used_block));
    size_t block_length = u->length + sizeof(struct used_block);
    printf("DEBUG: block from %d, length %d\n", (int)u, (int)block_length);

    for (struct list_elem *e = list_begin (&free_block_list);
         e != list_end (&free_block_list);
         e = list_next (e))
    {
        printf("DEBUG: block: %d\n", (int)&e);
    }

    struct free_block *f = (struct free_block *) u;
    f->length = block_length;
    printf("DEBUG: free_block: %d, length %d\n", (int)f, (int)(f->length));
    list_push_front(&free_block_list, &(f->elem));
    printf("DEBUG: list_size: %d\n", (int)list_size(&free_block_list));
}

/* Return the number of elements in the free list. */
size_t mem_sizeof_free_list(void)
{
    printf("DEBUG: LIST_SIZE: %d\n", (int)list_size(&free_block_list));
    return list_size(&free_block_list);
}

/* Dump the free list.  Implementation of this method is optional. */
void mem_dump_free_list(void)
{
    printf("DEBUG: DUMP_LIST: %d\n", (int)list_size(&free_block_list));

    printf("Dumping the free list:\n");
    for (struct list_elem *e = list_begin (&free_block_list);
         e != list_end (&free_block_list);
         e = list_next (e))
    {
        struct free_block *f = list_entry(e, struct free_block, elem);
        printf("block: length: %d, at %d\n", (int)(f->length), (int)f);
    }
    printf("Dumped.\n");
}
