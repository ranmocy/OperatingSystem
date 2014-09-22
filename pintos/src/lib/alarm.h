#ifndef __LIB_ALARM_H
#define __LIB_ALARM_H

#include <stdint.h>
#include <list.h>

/*
  Alarm struct is used to save the alarm in a linked list.
  So that we can save all the alarm and search in it,
  when interrupts happend.
*/
struct alarm
  {
    struct list_elem elem;  // list element
    int64_t ticks;          // target ticks to trigger the alarm
    struct thread *thread;  // the waiting thread
    unsigned magic;         // magic number to check alarm
  };

// init alarm list
void alarm_init (void);

// set alarm for current thread to wake up after TICKS
void alarm_create (int64_t ticks);

// check all alarms and wake up threads if time's up
void alarm_check (void);

#endif /* lib/alarm.h */
