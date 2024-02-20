#include <stdio.h>
#include <windows.h>
#define EOF (-1)
#define SLEEP_TIME 4*60000-10000 // 4 minutes 50 seconds

HANDLE gh_wait_event;
HANDLE gh_volume_thread;

void listen_for_keypress(void);
DWORD WINAPI toggle_volume(void *data);
LRESULT __stdcall keypress_callback(int n_code, WPARAM w_param, LPARAM l_param);

HHOOK _key_hook;

char *sleeping_computer = "           _________                      \n"
                          "          / ======= \\                   \n"
                          "         / __________\\                  \n"
                          "        | ___________ |                  \n"
                          "        | | -       | |                  \n"
                          "        | |         | |                  \n"
                          "        | |_________| |________________  \n"
                          "        \\=____________/      REM       ) \n"
                          "        / \"\"\"\"\"\"\"\"\"\"\" \\               /  \n"
                          "       / ::::::::::::: \\           =D-'   \n"
                          "      (_________________)                  \n";

void init_event_and_threads(void)
{
        // create an event we will use to signal to wait for thread
        gh_wait_event = CreateEvent(NULL, TRUE, FALSE, TEXT("gh_wait_event"));
        if (gh_wait_event == NULL) {
                printf("creating event failed! err: (%lu)\n", GetLastError());
                return;
        }

        // create a thread which will toggle volume every SLEEP_TIME seconds
        gh_volume_thread = CreateThread(NULL, 0, toggle_volume, NULL, 0, NULL);
        if (gh_volume_thread == NULL) {
                printf("creating volume thread failed! err: (%lu)\n", GetLastError());
                return;
        }
}

void init_state()
{
        if (!SetEvent(gh_wait_event))
                printf("signaling event state failed with err: (%lu)\n", GetLastError());
        return;
}

void close_event()
{
        CloseHandle(gh_wait_event);
}

int main()
{
        DWORD dw_wait_result;
        
        // init event, threads and initial state
        init_event_and_threads();
        init_state();

        // this will listen for keypresses that will toggle state depending on user input
        listen_for_keypress();

        while (1) {

                dw_wait_result = WaitForSingleObject(gh_volume_thread, INFINITE);
                switch (dw_wait_result) {
                        // the event is signaled
                        case WAIT_OBJECT_0:
                                break;
                        default:
                                printf("WaitForSingleObject() failed. err: (%lu)\n", GetLastError());
                                return 1;
                }
        }

        // clean up
        close_event();
        return 0;
}

DWORD WINAPI toggle_volume(void *data)
{
        DWORD dw_wait_result;
        while (1) {
                INPUT ip;
                ip.type = INPUT_KEYBOARD;

                // wait on event signal before proceeding with keypress
                dw_wait_result = WaitForSingleObject(gh_wait_event, INFINITE);
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

                // sleep thread for SLEEP_TIME seconds
                printf("thread %lu sleeping for %ums\n", GetCurrentThreadId(), SLEEP_TIME);
                printf("%s\n", sleeping_computer);
                Sleep(SLEEP_TIME);
        
        }
        printf("thread %lu exiting\n", GetLastError());
        return 1;
}

void listen_for_keypress(void)
{
        // create a hook for the keyboard
        _key_hook = SetWindowsHookExA(WH_KEYBOARD_LL, keypress_callback, NULL, 0);

        // check hook message
        MSG msg;
        while ( (GetMessage(&msg, NULL, 0, 0)) != 0) 
                Sleep(250);

        // unhook the global hook
        if (_key_hook) 
                UnhookWindowsHookEx(_key_hook);
        return;
}

LRESULT __stdcall keypress_callback(int n_code, WPARAM w_param, LPARAM l_param)
{
        PKBDLLHOOKSTRUCT key = (PKBDLLHOOKSTRUCT)l_param;

        // check keyboard key press
        if (w_param == WM_KEYDOWN && n_code == HC_ACTION) {
                switch (key->vkCode) {
                        case VK_F2:
                                printf("restarting REM after pressing F2\n");
                                if (!SetEvent(gh_wait_event)) {
                                        printf("signaling event failed with err: (%lu)\n", GetLastError());
                                        return CallNextHookEx(NULL, n_code, w_param, l_param);
                                }
                                break;
                        case VK_F12:
                                printf("paused REM after pressing F12\n");
                                if (!ResetEvent(gh_wait_event)) {
                                        printf("resetting event failed with err: (%lu)\n", GetLastError());
                                        return CallNextHookEx(NULL, n_code, w_param, l_param);
                                }
                                break;
                        default:
                                break;
                }
        }
        return CallNextHookEx(NULL, n_code, w_param, l_param);
}
