/* kbled IT829x keyboard backilight control
 * https://github.com/chememjc/kbled
 * Michael Curtis 2025-01-17
 * 
 * USB IT829x USB interface functions
 * Based off of USB protocol decode from https://github.com/matheusmoreira/ite-829x/tree/master
 */

#include <stdio.h>
#include <hidapi/hidapi.h>
#include "it829x.h"
#include "keymap.h"

hid_device *keyboard;  //global pointer to usb handle

uint8_t initcmd[7] = {0xCC, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x7F};  //command string to send a 'reset'
uint8_t brightspeedcmd[7] = {0xCC, 0x09, MAXBRIGHT, MAXSPEED, 0x00, 0x00,0x7F}; //command string to update the key brightness and speed
uint8_t setledcmd[7] = {0xCC, 0x01, K_ESC, RMIN, GMIN, BMIN, 0x7F}; //command string to set an individual LED's color
#define NUMMODES 7
uint8_t modecmd[7][7] = {
    {0xCC, 0x00, 0x04, 0x00, 0x00, 0x00, 0x7F},  //*     0    Wave
    {0xCC, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x7F},  //*     1    Breathe
    {0xCC, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x7F},  //*     2    Scan
    {0xCC, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x7F},  //*     3    Blink
    {0xCC, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00},  //*     4    Random
    {0xCC, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00},  //*     5    Ripple --doesn't seem to work on the bonw15 laptop for some reason
    {0xCC, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x53}}; //*     6    Snake

int8_t it829x_init(){
    keyboard = hid_open(VID, PID, NULL);
    if(keyboard==NULL){
        printf("failed to open USB port to %04x:%04x\n",VID,PID);
        return -1;
    }
    return 0;
}
int8_t it829x_close(){
    hid_close(keyboard);
    return 0;
}
int8_t it829x_reset(){
    return it829x_send(initcmd);
}
int8_t it829x_brightspeed(uint8_t bright, uint8_t speed){
    if(bright>0x0A || speed>0x02){
        printf("Out of range: bright(0x00-0x0A):0x%02x speed(0x00-0x02): 0x%02x\n",bright,speed);
        return -2;
    }
    brightspeedcmd[2]=bright;
    brightspeedcmd[3]=speed;
    return it829x_send(brightspeedcmd);
}
int8_t it829x_setleds(uint8_t *keys, uint8_t nkeys, uint8_t *color){
    int retval=0;
    for(uint8_t i=0; i<nkeys; i++){
        setledcmd[2]=keys[i];
        setledcmd[3]=color[0];
        setledcmd[4]=color[1];
        setledcmd[5]=color[2];
        retval+=it829x_send(setledcmd);
    }
    if(retval!=0) {
        printf("failed to send message %i times ",-retval);
        return -1; //return failure if any of the transactions failed
    }
    return 0; //success
}
int8_t it829x_effect(int8_t mode){
    if(mode>=0 && mode<NUMMODES) return it829x_send(modecmd[mode]);
    else{
        printf("Incorrect mode of %u: limits 0-%u\n",mode,NUMMODES-1);
        return -2;
    }
}
int8_t it829x_setled(uint8_t key, uint8_t *color){
    setledcmd[2]=key;
    setledcmd[3]=color[0];
    setledcmd[4]=color[1];
    setledcmd[5]=color[2];
    return it829x_send(setledcmd);
}
int8_t it829x_send(uint8_t *msg){
    if (hid_send_feature_report(keyboard, msg, sizeof(msg)) == -1){
        printf("failed to send message: ");
        for(unsigned int i=0;i<sizeof(msg);i++) printf("%02x ",msg[i]);
        printf("\n");
        return -1;
    }
    return 0; //successfull
}
