/* kbled IT829x keyboard backilight control
 * https://github.com/chememjc/kbled
 * Michael Curtis 2025-01-17
 * 
 * header for talking between daemon process and user update function
 */
#ifndef SHAREDMEM_H
#define SHAREDMEM_H

#include "keymap.h"
#include <stdint.h>

#define TOKEN_FILE "/var/run/kbled.ftok"  // File to store the ftok token
#define PROJECT_ID 'A'  // The project ID used to generate the ftok key

// shared_data status flags:
#define SM_B    0x0001  //brightness updated
#define SM_BI   0x0002  //brightness increment updated
#define SM_S    0x0004  //speed updated
#define SM_SI   0x0008  //speed increment updated
#define SM_E    0x0010  //effect updated
#define SM_EI   0x0020  //effect increment updated
#define SM_BL   0x0040  //backlight color updated
#define SM_FO   0x0080  //focus color updated
#define SM_KEY  0x0100  //individual key color updated
#define SM_SSPD 0x0200  //Scan speed updated

// shared_data key[3] update flags, set to toggle individual key state
#define SM_NOUPD    0   //no update
#define SM_UPD      1   //updated, read new R=key[0] G=key[1] B=key[2]
#define SM_BKGND    2   //use background color
#define SM_FOCUS    3   //use focus color

//Scan effects:
#define SM_EFFECT_NONE     -1
#define SM_EFFECT_WAVE      0
#define SM_EFFECT_BREATHE   1
#define SM_EFFECT_SCAN      2
#define SM_EFFECT_BLINK     3
#define SM_EFFECT_RANDOM    4
#define SM_EFFECT_RIPPLE    5  //doesn't seem to work on my bonw15/clevo x370 laptop with System76 firmware
#define SM_EFFECT_SNAKE     6

#define SEM_NAME "/kbled_semaphore"  // Semaphore name to synchronize access to shared memory
#define SEM_TIMEOUT_MS 1000 //semaphore timeout value 

// The structure of the shared memory segment
// Both programs can read from and write to this structure
struct shared_data {
    uint16_t status; //status flag, each binary bit represents a changed entry in the shared array
    double lastcputime; //contains the time in seconds it took to run through the last loop
    uint16_t scanspeed; //scan speed
    unsigned char brightness; //absolute brightness 0-10
    char brightnessinc;  //brightness increment +1 or -1, 0 for unchanged
    unsigned char speed; //absolute speed set 0-2
    char speedinc;  //speed increment +1 or -1, 0 for unchanged
    char effect; //absolute effect -1 to 6: -1=none, 0[Wave, Breathe, Scan, Blink, Random, Ripple, Snake]6  Note:ripple doesn't seem to work on bonw15
    char effectinc; //effect increment +1 or -1, 0 for unchanged
    unsigned char backlight[3]; //[R,G,B] 0-255 for each.  All keys
    unsigned char focus[3];  //[R,G,B] 0-255 for each, focus color (caps lock, num lock, scroll lock active)
    unsigned char key[NKEYS][4]; //RGB + update field for each key key[4] values are 0=no update, 1=updated, 2=use backlight color, 3=use focus color
};

extern struct shared_data *shm_ptr;


int sharedmem_init();
int sharedmem_close();
void sharedmem_lock();
void sharedmem_unlock();

#endif // SHARED_MEMORY_H
