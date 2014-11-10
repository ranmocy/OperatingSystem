#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#define USER_ADDRESS_BOTTOM ((void *) 0x08048000)
#define STACK_HEURISTIC 32
#define CLOSE_ALL -1

#include "vm/vm.h"

struct lock filesys_lock;

void syscall_init (void);
bool process_add_mmap (struct SP_entry *page_entry);
void process_remove_mmap (int mapping);

#endif /* userprog/syscall.h */
