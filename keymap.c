/* System76 bonw15 (Clevo X370)
 * Individually addressable keyboard LED daemon
 * Michael Curtis 2025-01-17
 * 
 * Based off of USB protocol decode from https://github.com/matheusmoreira/ite-829x/tree/master
 */
 #include <stdio.h>
 #include "keymap.h"
 unsigned char allkeys[]={K_ESC, K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12, K_PRINT_SCREEN, K_INSERT, K_DEL, K_HOME, K_END, K_PGUP, K_PGDN, K_TICK, K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9, K_0, K_MINUS, K_EQUALS, K_BKSPL, K_BKSPR, K_NUM_LOCK, K_NUM_SLASH, K_NUM_ASTERISK, K_NUM_MINUS, K_TABL, K_TABR, K_Q, K_W, K_E, K_R, K_T, K_Y, K_U, K_I, K_O, K_P, K_BRACE_OPEN, K_BRACE_CLOSE, K_BACKSLASH, K_NUM_7, K_NUM_8, K_NUM_9, K_NUM_PLUST, K_CAPSL, K_CAPSR, K_A, K_S, K_D, K_F, K_G, K_H, K_J, K_K, K_L, K_SEMICOLON, K_QUOTE, K_ENTERL, K_ENTERR, K_NUM_4, K_NUM_5, K_NUM_6, K_NUM_PLUSB, K_LEFT_SHIFTL, K_LEFT_SHIFTR, K_Z, K_X, K_C, K_V, K_B, K_N, K_M, K_COMMA, K_PERIOD, K_SLASH, K_RIGHT_SHIFTL, K_RIGHT_SHIFTR, K_UP, K_NUM_1, K_NUM_2, K_NUM_3, K_NUM_ENTERT, K_LEFT_CTRLL, K_LEFT_CTRLR, KT_FN, K_LEFT_SUPER, K_LEFT_ALT, K_SPACE1, K_SPACE2, K_SPACE3, K_SPACE4, K_RIGHT_ALT, K_APP, K_RIGHT_CTRLL, K_RIGHT_CTRLR, K_LEFT, K_DOWN, K_RIGHT, K_NUM_0, K_NUM_PERIOD, K_NUM_ENTERB};
 
 unsigned char findkey(unsigned char key){
    unsigned char i=0;
    while(i<NKEYS){
        if(allkeys[i]==key) return i; //return array index of key
        i++;
    }
    printf("Index out of range for key: %i\n",key);
    return 0;
 }
