#include <stdio.h>
#include <windows.h>
#include <ncurses/ncurses.h>
#include <time.h>
#include "timer.h"

//#define SLEEP_TIME 60000*5-10000 // 4 minutes 50 seconds
#define SLEEP_TIME 5000

// default window sizes
#define WINDOW_HEIGHT 18
#define WINDOW_WIDTH 45
#define WINDOW_START_Y 5
#define WINDOW_START_X 5

HHOOK KEY_HOOK;
HANDLE GH_MUTEX;
HANDLE WAIT_GH_EVENT;
HANDLE VOLUME_GH_THREAD;
HANDLE KEYPRESS_GH_THREAD;
WINDOW *WIN;
struct time_monitor TIMER; // declare global timer

// prototypes
void init_ncurses(void);
void draw(char *message);
void close_handles(void);
void init_threads(void);
DWORD WINAPI toggle_volume(LPVOID lp_param);
DWORD WINAPI listen_for_keypress(LPVOID lp_param);
LRESULT __stdcall keypress_callback(int n_code, WPARAM w_param, LPARAM l_param);

// default state
BOOL RUNNING = TRUE;
int main(void)
{
        // ncurses boilerplate
        init_ncurses();

        // create a shared mutex pointer that will be used amongst our threads
        GH_MUTEX = CreateMutex(NULL, FALSE, NULL);
        if (!GH_MUTEX) {
                printf("ERROR: could not create mutex: %lu\n", GetLastError());
                exit(1);
        }

        // init a time monitor to check state of time variables
        init_timer(&TIMER, GH_MUTEX);

        // init threads and event listeners
        init_threads();

        TIMER.time_until_next_sleep = SLEEP_TIME;
        while (1) 
        {
                if (!RUNNING) {
                        draw("paused");
                        continue;
                }

                // get current date to use as reference
                TIMER.now = time(0);

                // if they are the same, skip the iteration to preserve cpu cycles
                // this will only allow a full iteration once every second
                if (TIMER.now == TIMER.start)
                        continue;

                // reset our timer to be the current updated now time
                TIMER.start = TIMER.now;

                // get millisecond difference between now and the last time slept 
                //TIMER.time_diff_since_last_sleep = (TIMER.now - TIMER.time_last_slept) * 1000;
                if (RUNNING) {
                        WaitForSingleObject(TIMER.mutex, INFINITE);
                        TIMER.time_until_next_sleep -= 1000;
                        ReleaseMutex(TIMER.mutex);
                        draw("running");
                }
        }

        // clean up
        endwin();
        close_handles();
        return 0;
}

void draw(char *message)
{
        struct tm *tmp = gmtime(&TIMER.now);

        // modify terminal window
        mvwprintw(WIN, 1, 1, "           _________                     ");
        mvwprintw(WIN, 2, 1, "          / ======= \\                   ");
        mvwprintw(WIN, 3, 1, "         / __________\\                  ");
        mvwprintw(WIN, 4, 1, "        | ___________ |                  ");
        mvwprintw(WIN, 5, 1, "        | |  /\\_/\\  | |                  ");
        mvwprintw(WIN, 6, 1, "        | | ( o.o ) | |                  ");
        mvwprintw(WIN, 7, 1, "        | |  > ^ <  | |                  ");
        mvwprintw(WIN, 8, 1, "        | |_________| |________________  ");
        mvwprintw(WIN, 9, 1, "        \\=____________/      REM       ) ");
        mvwprintw(WIN, 10, 1, "        / \"\"\"\"\"\"\"\"\"\"\" \\               /  ");
        mvwprintw(WIN, 11, 1,"       / ::::::::::::: \\          =D-'   ");
        mvwprintw(WIN, 12, 1,"      (_________________)                  ");
        mvwprintw(WIN, 13, 1,"      current_time: %02d:%02d:%02dUTC", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
        mvwprintw(WIN, 14, 1,"      time until next sleep: %lld seconds ", (long long) TIMER.time_until_next_sleep/1000);
        //mvwprintw(WIN, 14, 1,"      time until next sleep: %lld seconds ", (long long) (SLEEP_TIME - TIMER.time_diff_since_last_sleep)/1000);
        mvwprintw(WIN, 15, 1,"      %s                             ", message);
        wrefresh(WIN);
}

void init_ncurses(void)
{
        // global TUI screen init settings
        initscr();
        cbreak();

        // init new window
        WIN = newwin(WINDOW_HEIGHT, WINDOW_WIDTH, WINDOW_START_Y, WINDOW_START_X);
        if (!WIN) {
                printf("ERROR: could not create window\n");
                exit(1);
        }
        nodelay(WIN, TRUE);
        curs_set(0); // hide cursor
        box(WIN, 0, 0); // create basic box borders
}

void close_handles(void)
{
        CloseHandle(WAIT_GH_EVENT);
        CloseHandle(VOLUME_GH_THREAD);
        CloseHandle(KEYPRESS_GH_THREAD);
        CloseHandle(GH_MUTEX);
}

void init_threads(void)
{
        // create an event we will use to signal to wait for thread
        WAIT_GH_EVENT = CreateEvent(NULL, TRUE, FALSE, TEXT("WAIT_GH_EVENT"));
        if (!WAIT_GH_EVENT) {
                printf("ERROR creating event failed! err: (%lu)\n", GetLastError());
                exit(1);
        }

        VOLUME_GH_THREAD = CreateThread(NULL, 0, toggle_volume, NULL, 0, NULL);
        if (!VOLUME_GH_THREAD) {
                printf("ERROR creating volume thread failed! err: (%lu)\n", GetLastError());
                exit(1);
        }

        // create thread which will listen for keypress to pause or continue REM
        KEYPRESS_GH_THREAD = CreateThread(NULL, 0, listen_for_keypress, NULL, 0, NULL);
        if (!KEYPRESS_GH_THREAD) {
                printf("ERROR: encountered error creating keypress listener thread. err: %lu\n", GetLastError());
        }
}

void init_event_state()
{
        // set the event
        if (!SetEvent(WAIT_GH_EVENT))
                printf("signaling event state failed with err: (%lu)\n", GetLastError());
}

DWORD WINAPI toggle_volume(LPVOID lp_param)
{
        UNREFERENCED_PARAMETER(lp_param);
        while (1) 
        {
                // if not running do nothing
                if (!RUNNING) {
                    continue;    
                }

                // if it is time to sleep, sleep
                //if ((long long) (SLEEP_TIME-TIMER.time_diff_since_last_sleep)/1000 <= 0) {
                if (TIMER.time_until_next_sleep == 0) {

                        INPUT ip;
                        ip.type = INPUT_KEYBOARD;
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

                        // sleep thread for SLEEP_TIME seconds
                        WaitForSingleObject(TIMER.mutex, INFINITE);
                        TIMER.time_until_next_sleep = SLEEP_TIME;
                        ReleaseMutex(TIMER.mutex);
                        Sleep(SLEEP_TIME);
                }
        }
        return 1;
}

DWORD WINAPI listen_for_keypress(LPVOID lp_param)
{
        UNREFERENCED_PARAMETER(lp_param);
        // create a hook for the keyboard
        KEY_HOOK = SetWindowsHookExA(WH_KEYBOARD_LL, keypress_callback, NULL, 0);

        // check hook message
        MSG msg;
        while ( (GetMessage(&msg, NULL, 0, 0)) != 0);

        // unhook the global hook
        if (KEY_HOOK) 
                UnhookWindowsHookEx(KEY_HOOK);
        return TRUE;
}

LRESULT __stdcall keypress_callback(int n_code, WPARAM w_param, LPARAM l_param)
{
        // check keyboard key press
        PKBDLLHOOKSTRUCT key = (PKBDLLHOOKSTRUCT)l_param;
        if (w_param == WM_KEYDOWN && n_code == HC_ACTION) {
                switch (key->vkCode) 
                {
                        // signal to resume the thread volume loop
                        case VK_F2:
                                if (!SetEvent(WAIT_GH_EVENT)) {
                                        printf("ERROR: signaling event failed with err: (%lu)\n", GetLastError());
                                        return CallNextHookEx(NULL, n_code, w_param, l_param);
                                }
                                // set loop state to run
                                RUNNING = TRUE;
                                break;

                        // signal to pause the thread volume loop
                        case VK_F12:
                                if (!ResetEvent(WAIT_GH_EVENT)) {
                                        printf("ERROR resetting event failed with err: (%lu)\n", GetLastError());
                                        return CallNextHookEx(NULL, n_code, w_param, l_param);
                                }
                                // set loop state to paused
                                RUNNING = FALSE;
                                break;
                        default:
                                break;
                }
        }
        return CallNextHookEx(NULL, n_code, w_param, l_param);
}

