/* kbled IT829x keyboard backilight control
 * https://github.com/chememjc/kbled
 * Michael Curtis 2025-01-17
 * 
 * Functions for determining caps lock, num lock and scroll lock state
 */

#include <stdio.h>
#include <unistd.h>  //for sleep function, open(), close() etc...
#include "kbstatus.h"


//Compile-time decision on mechanism to figure out if caps lock, num lock and scroll lock are active
#if defined(X11)  //***********************************************************
#include <X11/Xlib.h> //X11 libraries for looking at capslock, scroll lock and num lock
#include <X11/XKBlib.h>  //X11 libraries for looking at capslock, scroll lock and num lock
uint8_t kbstat() {
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Failed to open X display\n");
        return FAULT;
    }
    unsigned int state;
    if (XkbGetIndicatorState(display, XkbUseCoreKbd, &state) != Success) {
        fprintf(stderr, "Failed to get keyboard state\n");
        XCloseDisplay(display);
        return FAULT;
    }

    XCloseDisplay(display);
    return state;
}
#elif defined(IOCTL)  //*******************************************************
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
uint8_t kbstat() {
    int fd = open("/dev/console", O_RDONLY);
    if (fd < 0) {
        printf("Error opening console, is the executing user part of the tty group?\n");
        return FAULT;
    }
    uint8_t state;
    if (ioctl(fd, KDGETLED, &state) < 0) {
        printf("Error getting LED state?\n");
        close(fd);
        return FAULT;
    }
    close(fd);
    return state;  //order is different for ioctl, so return the same order as the X11 call
}
#elif defined(EVENT)  //*******************************************************
#include <fcntl.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/input.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>  // Required for DIR and directory functions

#define INPUT_DIR "/dev/input/"
#define EVENT_PREFIX "event"

char device_path[64]="X";

int is_keyboard(const char *device_path) {
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        perror("Error opening device");
        return 0;
    }

    // Get the device name
    char name[256];
    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
        perror("Error reading device name");
        close(fd);
        return 0;
    }

    close(fd);

    // Check if the device name contains "keyboard"
    return strstr(name, "keyboard") != NULL;
}

uint8_t check_led_states(const char *device_path) {
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        perror("Error opening input device for LED state");
        return FAULT;
    }

    unsigned long leds;
    if (ioctl(fd, EVIOCGLED(sizeof(leds)), &leds) < 0) {
        perror("Error getting LED state");
        close(fd);
        return FAULT;
    }

    close(fd);

    //printf("Caps Lock: %s\n", (leds & (1 << LED_CAPSL)) ? "ON" : "OFF");
    //printf("Num Lock: %s\n", (leds & (1 << LED_NUML)) ? "ON" : "OFF");
    //printf("Scroll Lock: %s\n", (leds & (1 << LED_SCROLLL)) ? "ON" : "OFF");
    uint8_t state=0;
    if(leds & (1 << LED_CAPSL)) state|= CAPLOC;
    if(leds & (1 << LED_NUML)) state|= NUMLOC;
    if(leds & (1 << LED_SCROLLL)) state|= SCRLOC;
    return state;
    
}

uint8_t kbfind() {
    DIR *dir = opendir(INPUT_DIR);
    if (!dir) {
        perror("Error opening /dev/input");
        return 1;
    }
    uint8_t state=FAULT;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, EVENT_PREFIX, strlen(EVENT_PREFIX)) == 0) {
            long unsigned int strsize=strlen(INPUT_DIR)+strlen(entry->d_name)+1;
            if(strsize<sizeof(device_path))snprintf(device_path, strsize, "%s%s", INPUT_DIR, entry->d_name);

            if (is_keyboard(device_path)) {
                printf("Keyboard found: %s\n", device_path);
                state=check_led_states(device_path);
                closedir(dir);
                return state; // Exit after finding the first keyboard
            }
        }
    }

    closedir(dir);
    fprintf(stderr, "No keyboard device found\n");
    return FAULT;
}
uint8_t kbstat() {
    if(device_path[0]=='X') return kbfind();
    return check_led_states(device_path);
}
#endif  //*********************************************************************
