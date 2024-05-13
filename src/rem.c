#include <stdio.h>
#include <windows.h>
#include <ncurses/ncurses.h>
#include <time.h>
#define SLEEP_TIME 5000
//#define SLEEP_TIME 60000*5-10000 // 4 minutes 50 seconds

// default window sizes
#define WINDOW_HEIGHT 16
#define WINDOW_WIDTH 45
#define WINDOW_START_Y 5
#define WINDOW_START_X 5

struct TIME_MONITOR {
        time_t time_last_slept; // the time the program last slept
        time_t start; // our relative timer start
        time_t time_diff;  // the time delta (in milliseconds) between now and the time last slept
        time_t now;
};

HHOOK _KEY_HOOK;
HANDLE WAIT_H_EVENT;
HANDLE VOLUME_H_THREAD;
HANDLE KEYPRESS_H_THREAD;
time_t TIME_LAST_SLEPT;
WINDOW *WIN;
struct TIME_MONITOR *TIMER; // declare global timer

// prototypes
void init_event_and_threads(void);
void init_event_state();
struct TIME_MONITOR* init_timer(struct TIME_MONITOR *time_monitor);
void destroy_timer(struct TIME_MONITOR *time_monitor);
void close_event();
void draw(char *message);
DWORD WINAPI listen_for_keypress(void *data);
DWORD WINAPI toggle_volume(void *data);
void init_window(void);
LRESULT __stdcall keypress_callback(int n_code, WPARAM w_param, LPARAM l_param);

int main(void)
{
        // init event, threads and initial event state
        init_event_and_threads();
        init_event_state();

        // global screen init
        initscr();
        cbreak();
        init_window();
        nodelay(WIN, TRUE);
        curs_set(0); // hide cursor in terminal

        // create basic box borders
        box(WIN, 0, 0);

        // init a time monitor to check state of time variables
        TIMER = init_timer(TIMER);
        while (1) {
                // capture current time and compare it to when we started the main loop
                TIMER->now = time(0);

                // if they are the same, skip the iteration
                if (TIMER->now == TIMER->start) 
                        continue;

                // reset our timer to be the current updated now time
                TIMER->start = TIMER->now;

                // get difference in ms between current and last sleep time
                TIMER->time_diff = (TIMER->now - TIMER->time_last_slept) * 1000;

                // reset time diff when timer runs out
                if ((long long) (SLEEP_TIME-TIMER->time_diff)/1000 <= 0)
                        TIMER->time_diff = 0;

                // draw the image with message
                draw("running");
        }

        // clean up
        endwin();
        close_event();
        destroy_timer(TIMER);
        return 0;
}

struct TIME_MONITOR* init_timer(struct TIME_MONITOR *time_monitor)
{
        TIMER = malloc(sizeof(struct TIME_MONITOR));
        if (!TIMER) {
                printf("ERROR: could not allocate enough memory for TIMER struct. exiting");
                exit(1);
        }

        // defaults
        TIMER->start = time(0);
        TIMER->time_last_slept = TIMER->start;
        return TIMER;
}

void destroy_timer(struct TIME_MONITOR *time_monitor)
{
        free(time_monitor);
}

void draw(char *message)
{
        struct tm *tmp = gmtime(&TIMER->now);
        //printf("time_diff: %lld\n", TIMER->time_diff);
        //printf("time until next sleep %lld\n", (long long) (SLEEP_TIME-TIMER->time_diff)/1000);

        // modify terminal window
        mvwprintw(WIN, 1, 1, "           _________                     ");
        mvwprintw(WIN, 2, 1, "          / ======= \\                   ");
        mvwprintw(WIN, 3, 1, "         / __________\\                  ");
        mvwprintw(WIN, 4, 1, "        | ___________ |                  ");
        mvwprintw(WIN, 5, 1, "        | | -       | |                  ");
        mvwprintw(WIN, 6, 1, "        | |         | |                  ");
        mvwprintw(WIN, 7, 1, "        | |_________| |________________  ");
        mvwprintw(WIN, 8, 1, "        \\=____________/      REM       ) ");
        mvwprintw(WIN, 9, 1, "        / \"\"\"\"\"\"\"\"\"\"\" \\             /  ");
        mvwprintw(WIN, 10, 1,"       / ::::::::::::: \\        =D-'   ");
        mvwprintw(WIN, 11, 1,"      (_________________)                  ");
        mvwprintw(WIN, 12, 1,"      current_time: %02d:%02d:%02dUTC", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
        mvwprintw(WIN, 13, 1,"      time until next sleep: %lld seconds ", (long long) (SLEEP_TIME-TIMER->time_diff)/1000);
        mvwprintw(WIN, 14, 1,"      %s                            ", message);
        wrefresh(WIN);
}

void init_event_and_threads(void)
{
        // create an event we will use to signal to wait for thread
        WAIT_H_EVENT = CreateEvent(NULL, TRUE, FALSE, TEXT("WAIT_H_EVENT"));
        if (!WAIT_H_EVENT) {
                printf("ERROR creating event failed! err: (%lu)\n", GetLastError());
                return;
        }

        // create a thread which will toggle volume every SLEEP_TIME seconds
        VOLUME_H_THREAD = CreateThread(NULL, 0, toggle_volume, NULL, 0, NULL);
        if (!VOLUME_H_THREAD) {
                printf("ERROR: creating volume thread failed! err: (%lu)\n", GetLastError());
                return;
        }

        // create thread which will listen for keypress to pause or continue REM
        KEYPRESS_H_THREAD = CreateThread(NULL, 0, listen_for_keypress, NULL, 0, NULL);
        if (!KEYPRESS_H_THREAD) {
                printf("ERROR: encountered error creating keypress listener thread. err: %lu\n", GetLastError());
        }
}

void init_event_state()
{
        if (!SetEvent(WAIT_H_EVENT))
                printf("ERROR: signaling event state failed with err: (%lu)\n", GetLastError());
        return;
}

void close_event()
{
        CloseHandle(WAIT_H_EVENT);
}

void init_window(void)
{
        WIN = newwin(WINDOW_HEIGHT, WINDOW_WIDTH, WINDOW_START_Y, WINDOW_START_X);
        if (!WIN) {
                printf("ERROR: could not create window\n");
                exit(1);
        }
}

DWORD WINAPI toggle_volume(void *data)
{
        DWORD dw_wait_result;
        while (1) {
                INPUT ip;
                ip.type = INPUT_KEYBOARD;

                // wait on event signal before proceeding with keypress
                dw_wait_result = WaitForSingleObject(WAIT_H_EVENT, INFINITE);
                switch (dw_wait_result) {
                        case WAIT_OBJECT_0:
                                break;
                        default:
                                printf("error waiting for event. err: (%lu)\n", GetLastError());
                                return 1;
                }

                ip.ki.wScan = 0;
                ip.ki.time = 0;
                ip.ki.dwExtraInfo = 0;

                // press virtual key for volume up
                ip.ki.wVk = VK_VOLUME_UP;
                ip.ki.dwFlags = 0;
                SendInput(1, &ip, sizeof(INPUT));

                // key release volume up
                ip.ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &ip, sizeof(INPUT));

                ip.ki.wVk = VK_VOLUME_DOWN;
                ip.ki.dwFlags = 0;
                SendInput(1, &ip, sizeof(INPUT));

                // key release volume down
                ip.ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &ip, sizeof(INPUT));

                // update timer with the time we are about to sleep
                TIMER->time_last_slept = time(0);

                // sleep thread for SLEEP_TIME seconds
                Sleep(SLEEP_TIME);
        
        }
        printf("ERROR: thread %lu exiting\n", GetLastError());
        return 1;
}

DWORD WINAPI listen_for_keypress(void *data)
{
        // create a hook for the keyboard
        _KEY_HOOK = SetWindowsHookExA(WH_KEYBOARD_LL, keypress_callback, NULL, 0);

        // check hook message
        MSG msg;
        while ( (GetMessage(&msg, NULL, 0, 0)) != 0) ;

        // unhook the global hook
        if (_KEY_HOOK) 
                UnhookWindowsHookEx(_KEY_HOOK);
        return TRUE;
}

LRESULT __stdcall keypress_callback(int n_code, WPARAM w_param, LPARAM l_param)
{
        // check keyboard key press
        PKBDLLHOOKSTRUCT key = (PKBDLLHOOKSTRUCT)l_param;
        if (w_param == WM_KEYDOWN && n_code == HC_ACTION) {
                switch (key->vkCode) {
                        case VK_F2:
                                TIMER->time_last_slept = time(0);
                                draw("running");
                                if (!SetEvent(WAIT_H_EVENT)) {
                                        printf("ERROR: signaling event failed with err: (%lu)\n", GetLastError());
                                        return CallNextHookEx(NULL, n_code, w_param, l_param);
                                }
                                break;
                        case VK_F12:
                                TIMER->time_last_slept = time(0);
                                draw("paused");
                                if (!ResetEvent(WAIT_H_EVENT)) {
                                        printf("ERROR resetting event failed with err: (%lu)\n", GetLastError());
                                        return CallNextHookEx(NULL, n_code, w_param, l_param);
                                }
                                break;
                        default:
                                break;
                }
        }
        return CallNextHookEx(NULL, n_code, w_param, l_param);
}
