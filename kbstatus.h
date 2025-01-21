/* System76 bonw15 (Clevo X370SNW)
 * USB individual RGB key backlight control
 * Subroutines for determining caps lock, num lock and scroll lock state
 */
#ifndef KBSTATUS_H
#define KBSTATUS_H

#include <stdint.h>  //uint8_t etc. definitions

#define FAULT  0x80  //fault flag

#if defined(X11)  //***********************************************************
#define CAPLOC 0x01
#define NUMLOC 0x02
#define SCRLOC 0x04
#elif defined(IOCTL)  //*******************************************************
#define CAPLOC 0x04
#define NUMLOC 0x02
#define SCRLOC 0x01
#elif defined(EVENT)  //*******************************************************
#define CAPLOC 0x01
#define NUMLOC 0x02
#define SCRLOC 0x04
#endif

uint8_t kbstat();

#endif
