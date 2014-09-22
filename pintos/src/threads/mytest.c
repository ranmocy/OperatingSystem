/* mytest.c */

#include <timer.h>
#include <stdio.h>
#include "thread.h"
#include "synch.h"
#include "mytest.h"

int x = 0;
struct semaphore s;
struct semaphore lock;


void thread_f(void *arg)
{
    long int id = (long int) arg;
    int i;

    printf("Semaphore in thread %ld.\n", id);

    for (i = 0; i < 100000; i++) {
        sema_down(&lock);
        x += 1;
        sema_up(&lock);
    }

    sema_up(&s);

    printf("End semaphore in thread %ld.\n", id);
}

void thread_sleep (void *arg)
{
    long int id = (long int) arg;

    printf ("Sleep in thread %ld.\n", id);
    int64_t ticks = 1000;
    timer_sleep (ticks); // Sleep 1 second
    printf ("Wake up in thread %ld\n", id);

    sema_up(&s);
}


void hello_world(void)
{
    tid_t t1, t2;


    printf("Test on semaphore:\n");
    sema_init(&s, 0);
    sema_init(&lock, 1);
    t1 = thread_create("thread0", 0, thread_f, (void *) 0);
    t2 = thread_create("thread1", 0, thread_f, (void *) 1);
    sema_down(&s);
    sema_down(&s);
    printf("x = %d\n", x);
    printf("Done on semaphore.\n\n");


    printf("Test on timer_sleep:\n");
    sema_init(&s, 0);
    t1 = thread_create ("thread0", 0, thread_sleep, (void *) 0);
    t2 = thread_create ("thread1", 0, thread_sleep, (void *) 1);
    sema_down(&s);
    sema_down(&s);
    printf("Done on timer_sleep\n\n");

}

