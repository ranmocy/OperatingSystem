#include <alarm.h>
#include <debug.h>
#include "devices/timer.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"

// struct alarm's signature
#define ALARM_MAGIC 0x31075264


static struct list alarm_list;


// init alarm list
void
alarm_init (void)
{
  list_init (&alarm_list);
  ASSERT (list_empty (&alarm_list));
}


// set alarm for current thread to wake up after TICKS
void
alarm_create (int64_t ticks)
{
  struct thread *thread = thread_current ();
  struct alarm *alarm = &thread->alarm;
  int64_t current_ticks = timer_ticks ();

  ASSERT (thread->status == THREAD_RUNNING);

  alarm->thread = thread;
  alarm->ticks = current_ticks + ticks;
  alarm->magic = ALARM_MAGIC;

  // Critical section, non-interruptable
  enum intr_level old_level = intr_disable ();

  list_push_back (&alarm_list, &alarm->elem);
  thread_block ();

  intr_set_level (old_level);
}

// destroy a alarm and wake the thread up
void
alarm_destroy (struct alarm *alarm)
{
  // ASSERT it's valid alarm
  ASSERT (alarm != NULL && alarm->magic == ALARM_MAGIC);

  // Critical section, non-interruptable
  enum intr_level old_level = intr_disable ();

  list_remove (&alarm->elem);
  thread_unblock (alarm->thread);

  intr_set_level (old_level);
}


// check all alarms and wake up threads if time's up
void
alarm_check (void)
{
  struct alarm *alarm;
  struct list_elem *e, *n;
  int64_t current_ticks = timer_ticks ();

  e = list_begin (&alarm_list);
  while (e != list_end (&alarm_list)) {
    alarm = list_entry (e, struct alarm, elem);
    n = list_next (e);

    if (alarm->ticks <= current_ticks) { // if time's up
      alarm_destroy (alarm);
    }

    e = n;
  }
}
