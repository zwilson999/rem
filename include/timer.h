#ifndef TIMER_H
#define TIMER_H

#include <time.h>
#include <windows.h>

struct time_monitor 
{
        time_t time_last_slept; // the time the program last slept
        time_t start; // our relative timer start
        time_t time_diff_since_last_sleep;  // the time delta (in milliseconds) between now and the time last slept
        time_t now;
        HANDLE mutex;
};

void init_timer(struct time_monitor *timer, HANDLE mutex);
void destroy_timer(struct time_monitor *timer);

#endif
