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


void *current_esp = NULL;

//
//                           ,,
//     `7MM"""Mq.            db                    mm
//       MM   `MM.                                 MM
//       MM   ,M9 `7Mb,od8 `7MM `7M'   `MF',6"Yb.mmMMmm .gP"Ya
//       MMmmdM9    MM' "'   MM   VA   ,V 8)   MM  MM  ,M'   Yb
//       MM         MM       MM    VA ,V   ,pm9MM  MM  8M""""""
//       MM         MM       MM     VVV   8M   MM  MM  YM.    ,
//     .JMML.     .JMML.   .JMML.    W    `Moo9^Yo.`Mbmo`Mbmmd'
//
//

int filesize (int fd);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

struct file* process_get_file (int fd);
int process_add_file (struct file *f);
void process_close_file (int fd);
bool process_add_mmap (struct SP_entry *page_entry);
void process_remove_mmap (int mapping);

int mmap (int fd, void *addr);
void munmap (int mapping);

static void syscall_exit (int rcode);

//
//                   ,,
//       .g8"""bgd `7MM                       `7MM
//     .dP'     `M   MM                         MM
//     dM'       `   MMpMMMb.  .gP"Ya   ,p6"bo  MM  ,MP'.gP"Ya `7Mb,od8
//     MM            MM    MM ,M'   Yb 6M'  OO  MM ;Y  ,M'   Yb  MM' "'
//     MM.           MM    MM 8M"""""" 8M       MM;Mm  8M""""""  MM
//     `Mb.     ,'   MM    MM YM.    , YM.    , MM `Mb.YM.    ,  MM
//       `"bmmmd'  .JMML  JMML.`Mbmmd'  YMbmd'.JMML. YA.`Mbmmd'.JMML.
//
//

static void
check_valid_pointer (const void *vaddr, const bool to_write)
{
    // if not a valid user addr, exit with ERROR
    if (!is_user_vaddr (vaddr) || vaddr < USER_ADDRESS_BOTTOM) {
        syscall_exit (ERROR);
    }
    // if can't find in pagedir and sup page table, exit with ERROR
    if (pagedir_get_page (thread_current ()->pagedir, vaddr) == NULL &&
        !page_find_and_load (vaddr, current_esp, to_write)) {
        syscall_exit (ERROR);
    }
}

static void
check_valid_buffer (const void* buffer, const unsigned size, const bool to_write)
{
    unsigned i;
    for (i = 0; i < size; i += PGSIZE) {
        check_valid_pointer (buffer + i, to_write);
    }
    check_valid_pointer (buffer + size - 1, to_write);
}

//
//                ,,    ,,
//     `7MM"""YMM db  `7MM
//       MM    `7       MM
//       MM   d `7MM    MM  .gP"Ya
//       MM""MM   MM    MM ,M'   Yb
//       MM   Y   MM    MM 8M""""""
//       MM       MM    MM YM.    ,
//     .JMML.   .JMML..JMML.`Mbmmd'
//
//

int
filesize (int fd)
{
    lock_acquire (&filesys_lock);
    struct file *f = process_get_file (fd);
    if (!f) {
        lock_release (&filesys_lock);
        return ERROR;
    }
    int size = file_length (f);
    lock_release (&filesys_lock);
    return size;
}

bool
create (const char *file, unsigned initial_size)
{
    lock_acquire (&filesys_lock);
    bool success = filesys_create (file, initial_size);
    lock_release (&filesys_lock);
    return success;
}

bool
remove (const char *file)
{
    lock_acquire (&filesys_lock);
    bool success = filesys_remove (file);
    lock_release (&filesys_lock);
    return success;
}

int
open (const char *file)
{
    lock_acquire (&filesys_lock);
    struct file *f = filesys_open (file);
    if (!f) {
        lock_release (&filesys_lock);
        return ERROR;
    }
    int fd = process_add_file (f);
    lock_release (&filesys_lock);
    return fd;
}

int
read (int fd, void *buffer, unsigned size)
{
    if (fd == STDIN_FILENO) {
        unsigned i;
        uint8_t* local_buffer = (uint8_t *)buffer;
        for (i = 0; i < size; i++) {
            local_buffer[i] = input_getc ();
        }
        return size;
    }
    lock_acquire (&filesys_lock);
    struct file *f = process_get_file (fd);
    if (!f) {
        lock_release (&filesys_lock);
        return ERROR;
    }
    int bytes = file_read (f, buffer, size);
    lock_release (&filesys_lock);
    return bytes;
}

int
write (int fd, const void *buffer, unsigned size)
{
    if (fd == STDOUT_FILENO) {
        putbuf (buffer, size);
        return size;
    }
    lock_acquire (&filesys_lock);
    struct file *f = process_get_file (fd);
    if (!f) {
        lock_release (&filesys_lock);
        return ERROR;
    }
    int bytes = file_write (f, buffer, size);
    lock_release (&filesys_lock);
    return bytes;
}

void
seek (int fd, unsigned position)
{
    lock_acquire (&filesys_lock);
    struct file *f = process_get_file (fd);
    if (!f) {
        lock_release (&filesys_lock);
        return;
    }
    file_seek (f, position);
    lock_release (&filesys_lock);
}

unsigned
tell (int fd)
{
    lock_acquire (&filesys_lock);
    struct file *f = process_get_file (fd);
    if (!f) {
        lock_release (&filesys_lock);
        return ERROR;
    }
    off_t offset = file_tell (f);
    lock_release (&filesys_lock);
    return offset;
}

void
close (int fd)
{
    lock_acquire (&filesys_lock);
    process_close_file (fd);
    lock_release (&filesys_lock);
}

//
//
//     `7MM"""Mq.
//       MM   `MM.
//       MM   ,M9 `7Mb,od8 ,pW"Wq.   ,p6"bo   .gP"Ya  ,pP"Ybd ,pP"Ybd
//       MMmmdM9    MM' "'6W'   `Wb 6M'  OO  ,M'   Yb 8I   `" 8I   `"
//       MM         MM    8M     M8 8M       8M"""""" `YMMMa. `YMMMa.
//       MM         MM    YA.   ,A9 YM.    , YM.    , L.   I8 L.   I8
//     .JMML.     .JMML.   `Ybmd9'   YMbmd'   `Mbmmd' M9mmmP' M9mmmP'
//
//

int
process_add_file (struct file *f)
{
    struct process_file_record *pf = malloc (sizeof (struct process_file_record));
    pf->file = f;
    pf->fd = thread_current ()->fd;
    thread_current ()->fd++;
    list_push_back (&thread_current ()->file_list, &pf->elem);
    return pf->fd;
}

struct file *
process_get_file (int fd)
{
    struct thread *t = thread_current ();
    struct list_elem *e;

    for (e = list_begin (&t->file_list); e != list_end (&t->file_list); e = list_next (e)) {
        struct process_file_record *pf = list_entry (e, struct process_file_record, elem);
        if (fd == pf->fd) {
            return pf->file;
        }
    }
    return NULL;
}

void
process_close_file (int fd)
{
    struct thread *t = thread_current ();
    struct list_elem *next, *e = list_begin (&t->file_list);

    while (e != list_end (&t->file_list)) {
        next = list_next (e);
        struct process_file_record *pf = list_entry (e, struct process_file_record, elem);
        if (fd == pf->fd || fd == CLOSE_ALL) {
            file_close (pf->file);
            list_remove (&pf->elem);
            free (pf);
            if (fd != CLOSE_ALL) {
                return;
            }
        }
        e = next;
    }
}

//
//
//     `7MMM.     ,MMF'
//       MMMb    dPMM
//       M YM   ,M MM  `7MMpMMMb.pMMMb.   ,6"Yb. `7MMpdMAo.
//       M  Mb  M' MM    MM    MM    MM  8)   MM   MM   `Wb
//       M  YM.P'  MM    MM    MM    MM   ,pm9MM   MM    M8
//       M  `YM'   MM    MM    MM    MM  8M   MM   MM   ,AP
//     .JML. `'  .JMML..JMML  JMML  JMML.`Moo9^Yo. MMbmmd'
//                                                 MM
//                                               .JMML.

int
mmap (int fd, void *addr)
{
    struct file *old_file = process_get_file (fd);
    if (!old_file || !is_user_vaddr (addr) || addr < USER_ADDRESS_BOTTOM ||
        ( (uint32_t) addr % PGSIZE) != 0) {
        return ERROR;
    }
    struct file *file = file_reopen (old_file);
    if (!file || file_length (old_file) == 0) {
        return ERROR;
    }
    thread_current ()->mapid++;
    int32_t ofs = 0;
    uint32_t read_bytes = file_length (file);
    while (read_bytes > 0) {
        uint32_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        uint32_t page_zero_bytes = PGSIZE - page_read_bytes;
        if (!page_add_mmap (file, ofs, addr, page_read_bytes, page_zero_bytes)) {
            munmap (thread_current ()->mapid);
            return ERROR;
        }
        read_bytes -= page_read_bytes;
        ofs += page_read_bytes;
        addr += PGSIZE;
    }
    return thread_current ()->mapid;
}

void munmap (int mapping)
{
    process_remove_mmap (mapping);
}

//
//                                              ,,    ,,
//     `7MMF'  `7MMF'                         `7MM  `7MM
//       MM      MM                             MM    MM
//       MM      MM   ,6"Yb.  `7MMpMMMb.   ,M""bMM    MM  .gP"Ya `7Mb,od8
//       MMmmmmmmMM  8)   MM    MM    MM ,AP    MM    MM ,M'   Yb  MM' "'
//       MM      MM   ,pm9MM    MM    MM 8MI    MM    MM 8M""""""  MM
//       MM      MM  8M   MM    MM    MM `Mb    MM    MM YM.    ,  MM
//     .JMML.  .JMML.`Moo9^Yo..JMML  JMML.`Wbmd"MML..JMML.`Mbmmd'.JMML.
//
//

static void
syscall_exit (int rcode){
    struct list_elem* e;
    struct thread* cur = thread_current ();
    while (!list_empty (&cur->file_list))
    {
        e = list_begin (&cur->file_list);
        close (list_entry (e, struct process_file_record, elem)->fd);
    }
    printf ("%s: exit(%d)\n", cur->name, rcode);
    cur->ret = rcode;
    thread_exit ();
}

static int arg_count[] = {0, 1, 1, 1, 2, 1, 1, 1, 3, 3, 2, 1, 1, 2, 1};
static void
syscall_handler (struct intr_frame *f)
{
    current_esp = f->esp;

    void **p = f->esp; // parameters are a array of anything
    check_valid_pointer (p, false); // check the first parameter pointer
    int event_id = (int)p[0];
    ASSERT (event_id <= 14 && "We only support syscall from 0 to 14.");
    check_valid_pointer (p + arg_count[event_id], false); // check the last

    switch (event_id) {
        case SYS_HALT:{ // 0
            shutdown_power_off ();
            break;
        }
        case SYS_EXIT:{ // 1, code
            syscall_exit ((int)p[1]);
            break;
        }
        case SYS_EXEC: { // 2, filename
            check_valid_pointer (p[1], false);
            f->eax = process_execute (p[1]);
            break;
        }
        case SYS_WAIT: { // 3, tid
            f->eax = process_wait ((tid_t)p[1]);
            break;
        }
        case SYS_CREATE: { // 4, filename, size
            check_valid_pointer (p[1], false);
            f->eax = create ((char *)p[1], (unsigned)p[2]);
            break;
        }
        case SYS_REMOVE: { // 5, filename
            check_valid_pointer (p[1], false);
            f->eax = remove ((char *)p[1]);
            break;
        }
        case SYS_OPEN: { // 6, filename
            check_valid_pointer (p[1], false);
            f->eax = open ((char *)p[1]);
            break;
        }
        case SYS_FILESIZE: { // 7, fd
            f->eax = filesize ((int)p[1]);
            break;
        }
        case SYS_READ: { // 8, fd, *buffer, size
            check_valid_buffer (p[2], (unsigned)p[3], true);
            f->eax = read ((int)p[1], p[2], (unsigned)p[3]);
            break;
        }
        case SYS_WRITE: { // 9, fd, *buffer, size
            check_valid_buffer (p[2], (unsigned)p[3], false);
            if ((int)p[1] == 1) { // STDOUT
                putbuf ((char *)p[2], (size_t)p[3]);
            } else{
                f->eax = write ((int)p[1], p[2], (unsigned)p[3]);
            }
            break;
        }
        case SYS_SEEK: { // 10, fd, position
            seek ((int)p[1], (unsigned)p[2]);
            break;
        }
        case SYS_TELL: { // 11, fd
            f->eax = tell ((int)p[1]);
            break;
        }
        case SYS_CLOSE: { // 12, fd
            close ((int)p[1]);
            break;
        }
        case SYS_MMAP: { // 13, fd, addr
            check_valid_pointer (p[2], false);
            f->eax = mmap ((int)p[1], (void *) p[2]);
            break;
        }
        case SYS_MUNMAP: { // 14, fd
            munmap ((int)p[1]);
            break;
        }
        default: {
            printf ("Oops\n");
            thread_exit ();
        }
    }
}

//
//                            ,,        ,,    ,,
//     `7MM"""Mq.            *MM      `7MM    db
//       MM   `MM.            MM        MM
//       MM   ,M9 `7MM  `7MM  MM,dMMb.  MM  `7MM  ,p6"bo
//       MMmmdM9    MM    MM  MM    `Mb MM    MM 6M'  OO
//       MM         MM    MM  MM     M8 MM    MM 8M
//       MM         MM    MM  MM.   ,M9 MM    MM YM.    ,
//     .JMML.       `Mbod"YML.P^YbmdP'.JMML..JMML.YMbmd'
//
//

void
syscall_init (void)
{
    lock_init (&filesys_lock);
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool
process_add_mmap (struct SP_entry *page_entry)
{
    struct process_mmap_record *mm = malloc (sizeof (struct process_mmap_record));
    if (!mm) {
        return false;
    }
    mm->page_entry = page_entry;
    mm->mapid = thread_current ()->mapid;
    list_push_back (&thread_current ()->mmap_list, &mm->elem);
    return true;
}

void
process_remove_mmap (int mapping)
{
    struct thread *t = thread_current ();
    struct list_elem *next, *e = list_begin (&t->mmap_list);
    struct file *f = NULL;
    int close = 0;

    while (e != list_end (&t->mmap_list)) {
        next = list_next (e);
        struct process_mmap_record *mm = list_entry (e, struct process_mmap_record, elem);
        if (mm->mapid == mapping || mapping == CLOSE_ALL) {
            mm->page_entry->pinned = true;
            if (mm->page_entry->is_loaded) {
                if (pagedir_is_dirty (t->pagedir, mm->page_entry->page)) {
                    lock_acquire (&filesys_lock);
                    file_write_at (mm->page_entry->file, mm->page_entry->page,
                                   mm->page_entry->read_bytes, mm->page_entry->offset);
                    lock_release (&filesys_lock);
                }
                frame_free (pagedir_get_page (t->pagedir, mm->page_entry->page));
                pagedir_clear_page (t->pagedir, mm->page_entry->page);
            }
            if (mm->page_entry->type != SP_ERROR) {
                hash_delete (&t->page_table, &mm->page_entry->elem);
            }
            list_remove (&mm->elem);
            if (mm->mapid != close) {
                if (f) {
                    lock_acquire (&filesys_lock);
                    file_close (f);
                    lock_release (&filesys_lock);
                }
                close = mm->mapid;
                f = mm->page_entry->file;
            }
            free (mm->page_entry);
            free (mm);
        }
        e = next;
    }
    if (f) {
        lock_acquire (&filesys_lock);
        file_close (f);
        lock_release (&filesys_lock);
    }
}
