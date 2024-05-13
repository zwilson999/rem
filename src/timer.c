#include "timer.h"
#include <windows.h>

void init_timer(struct time_monitor *timer, HANDLE mut)
{
        // defaults
        timer->start = time(0);
        timer->time_last_slept = timer->start;
        timer->time_diff_since_last_sleep = 0;
        timer->mutex = mut;
}
