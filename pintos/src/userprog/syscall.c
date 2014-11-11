#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#define ERROR -1

static void syscall_handler(struct intr_frame *);
static void check_valid_pointer(const void *vaddr);
static void* vaddr_to_phyaddr(void *vaddr);
static void syscall_exit(int rcode);
void check_valid_buffer(void* buffer, unsigned size);
struct file* process_get_file(int fd);
unsigned tell(int fd);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int process_add_file(struct file *f);
void process_close_file(int fd);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
void close(int fd);

bool process_add_mmap (struct SP_entry *page_entry);
void process_remove_mmap (int mapping);
int mmap (int fd, void *addr);
void munmap (int mapping);


struct process_file_record {
    struct file *file;
    int fd;
    struct list_elem elem;
};

struct process_mmap_record {
    struct SP_entry *page_entry;
    int mapid;
    struct list_elem elem;
};

void
syscall_init(void)
{
    lock_init(&filesys_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

int arg_count[] = {0, 1, 1, 1, 2, 1, 1, 1, 3, 3, 2, 1, 1};

static void
syscall_handler(struct intr_frame *f)
{

    int *p = (int*)f->esp;
    int addr;
    check_valid_pointer(p);
    check_valid_pointer(p + arg_count[*p]);
    switch (*p){
    case SYS_CREATE:
        addr = vaddr_to_phyaddr(p[1]);
        f->eax = create((const char *)addr, (unsigned)p[2]);
        break;
    case SYS_REMOVE:
        addr = vaddr_to_phyaddr(p[1]);
        f->eax = remove((const char *)addr);
        break;
    case SYS_OPEN:
        addr = vaddr_to_phyaddr(p[1]);
        f->eax = open((const char *)addr);
        break;
    case SYS_FILESIZE:
        f->eax = filesize(p[1]);
        break;
    case SYS_READ: // fd, *buffer, size
        check_valid_buffer((void *)p[2], (unsigned)p[3]);
        addr = vaddr_to_phyaddr(p[2]);
        f->eax = read(p[1], addr, (unsigned)p[3]);
        break;
    case SYS_WRITE: // fd, *buffer, size
        check_valid_buffer((void *)p[2], (unsigned)p[3]);
        if (p[1] == 1){
                putbuf((const char*)p[2], p[3]);
        }
        else{
            addr = vaddr_to_phyaddr(p[2]);
            f->eax = write(p[1], addr, (unsigned)p[3]);
        }
        break;
    case SYS_SEEK:
        seek(p[1], (unsigned)p[2]);
        break;
    case SYS_TELL:
        f->eax = tell(p[1]);
        break;
    case SYS_CLOSE:
        close(p[1]);
        break;
    case SYS_WAIT:
        f->eax = process_wait(p[1]);
        break;
    case SYS_EXIT:
        syscall_exit(p[1]);
        break;
    case SYS_HALT:
        shutdown_power_off();
        break;
    case SYS_EXEC:
        f->eax = process_execute(vaddr_to_phyaddr((void*)p[1]));
        break;
    case SYS_MMAP: {
        f->eax = mmap (p[2], (void *) p[1]);
        break;
    }
    case SYS_MUNMAP: {
        munmap (p[0]);
        break;
    }
    default:
        printf("Oops\n");
        thread_exit();
    }
}

static void
check_valid_pointer(const void *vaddr)
{
    if (!is_user_vaddr(vaddr) || vaddr < USER_ADDRESS_BOTTOM ||
        pagedir_get_page(thread_current()->pagedir,vaddr) == NULL ||
        !page_find (vaddr)){
        syscall_exit(ERROR);
    }
}

static void*
vaddr_to_phyaddr(void *vaddr){
    // TO DO: verify the virtual memory exist, if exist return the physical address
    //        else exist with ERROR.
    check_valid_pointer(vaddr);
    void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
    if (!ptr) {
        syscall_exit(ERROR);
    }
    return ptr;
}

static void
syscall_exit(int rcode){
    struct list_elem* e;
    struct thread* cur = thread_current();
    while (!list_empty(&cur->file_list))
    {
        e = list_begin(&cur->file_list);
        close(list_entry(e, struct process_file_record, elem)->fd);
    }
    printf("%s: exit(%d)\n", cur->name, rcode);
    cur->ret = rcode;
    thread_exit();
}

bool create(const char *file, unsigned initial_size)
{
    lock_acquire(&filesys_lock);
    bool success = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
    return success;
}

bool remove(const char *file)
{
    lock_acquire(&filesys_lock);
    bool success = filesys_remove(file);
    lock_release(&filesys_lock);
    return success;
}

int open(const char *file)
{
    lock_acquire(&filesys_lock);
    struct file *f = filesys_open(file);
    if (!f)
    {
        lock_release(&filesys_lock);
        return ERROR;
    }
    int fd = process_add_file(f);
    lock_release(&filesys_lock);
    return fd;
}


int process_add_file(struct file *f)
{
    struct process_file_record *pf = malloc(sizeof(struct process_file_record));
    pf->file = f;
    pf->fd = thread_current()->fd;
    thread_current()->fd++;
    list_push_back(&thread_current()->file_list, &pf->elem);
    return pf->fd;
}

int filesize(int fd)
{
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    if (!f)
    {
        lock_release(&filesys_lock);
        return ERROR;
    }
    int size = file_length(f);
    lock_release(&filesys_lock);
    return size;
}

int read(int fd, void *buffer, unsigned size)
{
    if (fd == STDIN_FILENO)
    {
        unsigned i;
        uint8_t* local_buffer = (uint8_t *)buffer;
        for (i = 0; i < size; i++)
        {
            local_buffer[i] = input_getc();
        }
        return size;
    }
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    if (!f)
    {
        lock_release(&filesys_lock);
        return ERROR;
    }
    int bytes = file_read(f, buffer, size);
    lock_release(&filesys_lock);
    return bytes;
}




int write(int fd, const void *buffer, unsigned size)
{
    if (fd == STDOUT_FILENO)
    {
        putbuf(buffer, size);
        return size;
    }
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    if (!f)
    {
        lock_release(&filesys_lock);
        return ERROR;
    }
    int bytes = file_write(f, buffer, size);
    lock_release(&filesys_lock);
    return bytes;
}



struct file* process_get_file(int fd)
{
    struct thread *t = thread_current();
    struct list_elem *e;

    for (e = list_begin(&t->file_list); e != list_end(&t->file_list);
        e = list_next(e))
    {
        struct process_file_record *pf = list_entry(e, struct process_file_record, elem);
        if (fd == pf->fd)
        {
            return pf->file;
        }
    }
    return NULL;
}


void process_close_file(int fd)
{
    struct thread *t = thread_current();
    struct list_elem *next, *e = list_begin(&t->file_list);

    while (e != list_end(&t->file_list))
    {
        next = list_next(e);
        struct process_file_record *pf = list_entry(e, struct process_file_record, elem);
        if (fd == pf->fd || fd == CLOSE_ALL)
        {
            file_close(pf->file);
            list_remove(&pf->elem);
            free(pf);
            if (fd != CLOSE_ALL)
            {
                return;
            }
        }
        e = next;
    }
}

bool process_add_mmap (struct SP_entry *page_entry)
{
  struct process_mmap_record *mm = malloc(sizeof(struct process_mmap_record));
  if (!mm)
    {
      return false;
    }
  mm->page_entry = page_entry;
  mm->mapid = thread_current()->mapid;
  list_push_back(&thread_current()->mmap_list, &mm->elem);
  return true;
}

void process_remove_mmap (int mapping)
{
  struct thread *t = thread_current();
  struct list_elem *next, *e = list_begin(&t->mmap_list);
  struct file *f = NULL;
  int close = 0;

  while (e != list_end (&t->mmap_list))
    {
      next = list_next(e);
      struct process_mmap_record *mm = list_entry (e, struct process_mmap_record, elem);
      if (mm->mapid == mapping || mapping == CLOSE_ALL)
        {
          mm->page_entry->pinned = true;
          if (mm->page_entry->is_loaded)
            {
              if (pagedir_is_dirty(t->pagedir, mm->page_entry->page))
                {
                  lock_acquire(&filesys_lock);
                  file_write_at(mm->page_entry->file, mm->page_entry->page,
                                mm->page_entry->read_bytes, mm->page_entry->offset);
                  lock_release(&filesys_lock);
                }
              frame_free(pagedir_get_page(t->pagedir, mm->page_entry->page));
              pagedir_clear_page(t->pagedir, mm->page_entry->page);
            }
          if (mm->page_entry->type != SP_ERROR)
            {
              hash_delete(&t->page_table, &mm->page_entry->elem);
            }
          list_remove(&mm->elem);
          if (mm->mapid != close)
            {
              if (f)
                {
                  lock_acquire(&filesys_lock);
                  file_close(f);
                  lock_release(&filesys_lock);
                }
              close = mm->mapid;
              f = mm->page_entry->file;
            }
          free(mm->page_entry);
          free(mm);
        }
      e = next;
    }
  if (f)
    {
      lock_acquire(&filesys_lock);
      file_close(f);
      lock_release(&filesys_lock);
    }
}

int mmap (int fd, void *addr)
{
    struct file *old_file = process_get_file (fd);
    if (!old_file || !is_user_vaddr (addr) || addr < USER_ADDRESS_BOTTOM ||
        ((uint32_t) addr % PGSIZE) != 0) {
        return ERROR;
    }
    struct file *file = file_reopen (old_file);
    if (!file || file_length (old_file) == 0) {
        return ERROR;
    }
    thread_current()->mapid++;
    int32_t ofs = 0;
    uint32_t read_bytes = file_length (file);
    while (read_bytes > 0) {
        uint32_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        uint32_t page_zero_bytes = PGSIZE - page_read_bytes;
        if (!page_add_mmap(file, ofs, addr, page_read_bytes, page_zero_bytes)) {
            munmap (thread_current()->mapid);
            return ERROR;
        }
        read_bytes -= page_read_bytes;
        ofs += page_read_bytes;
        addr += PGSIZE;
    }
    return thread_current()->mapid;
}

void munmap (int mapping)
{
    process_remove_mmap(mapping);
}




void seek(int fd, unsigned position)
{
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    if (!f)
    {
        lock_release(&filesys_lock);
        return;
    }
    file_seek(f, position);
    lock_release(&filesys_lock);
}

unsigned tell(int fd)
{
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    if (!f)
    {
        lock_release(&filesys_lock);
        return ERROR;
    }
    off_t offset = file_tell(f);
    lock_release(&filesys_lock);
    return offset;
}

void close(int fd)
{
    lock_acquire(&filesys_lock);
    process_close_file(fd);
    lock_release(&filesys_lock);
}

void check_valid_buffer(void* buffer, unsigned size)
{
    unsigned i;
    char* local_buffer = (char *)buffer;
    for (i = 0; i < size; i++)
    {
        check_valid_pointer((const void*)local_buffer);
        local_buffer++;
    }
}
