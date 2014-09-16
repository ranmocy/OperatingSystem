/* mytest.c */

#include <stdio.h>
#include "thread.h"
#include "synch.h"
#include "mytest.h"

int x = 0;
struct semaphore s;
struct semaphore lock;

void thread_f(void *arg);

void thread_f(void *arg)
{
    long int id = (long int) arg;
    int i;

    printf("Hello from thread %ld.\n", id);

    for (i = 0; i < 100000; i++) {
        sema_down(&lock);
        x += 1;
        sema_up(&lock);
    }

    sema_up(&s);
}


void hello_world(void)
{
    tid_t t1, t2;

    printf("Hello World.\n");

    sema_init(&s, 0);
    sema_init(&lock, 1);

    t1 = thread_create("thread0", 0, thread_f, (void *) 0);
    t2 = thread_create("thread1", 0, thread_f, (void *) 1);

    sema_down(&s);
    sema_down(&s);

    printf("x = %d\n", x);

    printf("Hello World Done.\n");
}

