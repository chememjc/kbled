/* System76 bonw15 (Clevo X370SNW)
 * USB individual RGB key backlight control
 * header for talking between daemon process and user update function
 */
#ifndef SHAREDMEM_H
#define SHAREDMEM_H

#include "keymap.h"
#include <stdint.h>

#define TOKEN_FILE "/var/run/kbled.ftok"  // File to store the ftok token
#define PROJECT_ID 'A'  // The project ID used to generate the ftok key

#define SM_B    0x0001  //brightness updated
#define SM_BI   0x0002  //brightness increment updated
#define SM_S    0x0004  //speed updated
#define SM_SI   0x0008  //speed increment updated
#define SM_E    0x0010  //effect updated
#define SM_EI   0x0020  //effect increment updated
#define SM_BL   0x0040  //backlight color updated
#define SM_FO   0x0080  //focus color updated
#define SM_KEY  0x0100  //individual key color updated

#define SM_NOUPD    0   //no update
#define SM_UPD      1   //updated
#define SM_BKGND    2   //use background color
#define SM_FOCUS    3   //use focus color

#define SEM_NAME "/kbled_semaphore"  // Semaphore name to synchronize access to shared memory
#define SEM_TIMEOUT_MS 1000 //semaphore timeout value 

// The structure of the shared memory segment
// Both programs can read from and write to this structure
struct shared_data {
    uint16_t status; //status flag; 0b0000000000000000 where 
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
