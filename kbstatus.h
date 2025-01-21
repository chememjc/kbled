/* kbled IT829x keyboard backilight control
 * https://github.com/chememjc/kbled
 * Michael Curtis 2025-01-17
 * 
 * Functions for determining caps lock, num lock and scroll lock state
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
