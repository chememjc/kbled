/* System76 bonw15 (Clevo X370SNW)
 * USB individual RGB key backlight control
 *
 */
#ifndef IT829X_H
#define IT829X_H

//USB Identifier Strings:
//from lsusb:  ID 048d:8910 Integrated Technology Express, Inc. ITE Device(829x)
#define VID 0x048d
#define PID 0x8910

#define MAXBRIGHT 0x0A
#define MINBRIGHT 0x00
#define MAXSPEED  0x02
#define MINSPEED  0x00
#define MAXEFFECT 6
#define MINEFFECT -1
#define RMAX 0xFF
#define RMIN 0x00
#define GMAX 0xFF
#define GMIN 0x00
#define BMAX 0xFF
#define BMIN 0x00

#include <hidapi/hidapi.h>
#include <stdint.h>  //uint8_t etc. definitions

extern hid_device *keyboard;

int8_t it829x_init();
void it829x_close();
int8_t it829x_reset();
int8_t it829x_brightspeed(uint8_t bright, uint8_t speed);
int8_t it829x_setleds(uint8_t *keys, uint8_t nkeys, uint8_t *color);
int8_t it829x_setled(uint8_t key, uint8_t *color);
int8_t it829x_send(uint8_t *msg);

#endif
