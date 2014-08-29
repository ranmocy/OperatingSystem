#include <stdio.h>
#include "memalloc.h"

struct list free_block_list;


// Initialize memory allocator to use 'length' bytes of memory at 'base'.
void mem_init(uint8_t *base, size_t length)
{
    list_init(&free_block_list);

}

// Allocate 'length' bytes of memory.
void * mem_alloc(size_t length)
{
    return NULL;
}

// Free memory pointed to by 'ptr'.
void mem_free(void *ptr)
{

}

// Return the number of elements in the free list.
size_t mem_sizeof_free_list(void)
{
    return (size_t) 0;
}

// Dump the free list.
void mem_dump_free_list(void)
{

}
