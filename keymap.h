/* kbled IT829x keyboard backilight control
 * https://github.com/chememjc/kbled
 * Michael Curtis 2025-01-17
 * 
 * basic keymap found in System76 EC firmware layout (https://github.com/system76/ec/blob/master/src/keyboard/system76/15in_102/keymap/default.c)
 * Keys with mulitple LEDs, 2x unless specified (backspace, tab, keypad plus, caps lock, enter, lshift, rshift, keypad enter, lctrl, spacebar (4x), rctrl)
 * These cases were added to address each LED individually with L and R modifiers for left and right except the space where they are numbered 1-4 from left to right
 */
 
#ifndef KEYMAP_H
#define KEYMAP_H

#define NKEYS 115
extern unsigned char allkeys[NKEYS]; //array of all the keys on the keyboard
unsigned char findkey(unsigned char key);  //function to return index of each led/key

//      Key Name        LED Address 
//row 1
#define K_ESC           0
#define K_F1            1
#define K_F2            2
#define K_F3            3
#define K_F4            4
#define K_F5            5
#define K_F6            6
#define K_F7            7
#define K_F8            8
#define K_F9            9
#define K_F10           10
#define K_F11           11
#define K_F12           12
#define K_PRINT_SCREEN  13
#define K_INSERT        14
#define K_DEL           15
#define K_HOME          16
#define K_END           17
#define K_PGUP          18
#define K_PGDN          19
//row 2
#define K_TICK          32
#define K_1             33
#define K_2             34
#define K_3             35
#define K_4             36
#define K_5             37
#define K_6             38
#define K_7             39
#define K_8             40
#define K_9             41
#define K_0             42
#define K_MINUS         43
#define K_EQUALS        45
#define K_BKSPL         46  // left of key
#define K_BKSPR         47  // right of key
#define K_NUM_LOCK      48
#define K_NUM_SLASH     49
#define K_NUM_ASTERISK  50
#define K_NUM_MINUS     51
//row 3
#define K_TABL          64  // left of key
#define K_TABR          65  // right of key
#define K_Q             66
#define K_W             67
#define K_E             68
#define K_R             69
#define K_T             70
#define K_Y             71
#define K_U             72
#define K_I             73
#define K_O             74
#define K_P             75
#define K_BRACE_OPEN    76
#define K_BRACE_CLOSE   77
#define K_BACKSLASH     78
#define K_NUM_7         80
#define K_NUM_8         81
#define K_NUM_9         82
#define K_NUM_PLUST     83  // top of key
//row 4
#define K_CAPSL         96  // left of key
#define K_CAPSR         97  // right of key
#define K_A             98
#define K_S             99
#define K_D             100
#define K_F             101
#define K_G             102
#define K_H             103
#define K_J             104
#define K_K             105
#define K_L             106
#define K_SEMICOLON     107
#define K_QUOTE         108
#define K_ENTERL        110  // left of key
#define K_ENTERR        111  // right of key
#define K_NUM_4         112
#define K_NUM_5         113
#define K_NUM_6         114
#define K_NUM_PLUSB     115  // bottom of key
//row 5
#define K_LEFT_SHIFTL   128  // left of key
#define K_LEFT_SHIFTR   130  // right of key
#define K_Z             131
#define K_X             132
#define K_C             133
#define K_V             134
#define K_B             135
#define K_N             136
#define K_M             137
#define K_COMMA         138
#define K_PERIOD        139
#define K_SLASH         140
#define K_RIGHT_SHIFTL  141  // left of key
#define K_RIGHT_SHIFTR  142  // right of key
#define K_UP            143
#define K_NUM_1         144
#define K_NUM_2         145
#define K_NUM_3         146
#define K_NUM_ENTERT    147  // top of key
//row 6
#define K_LEFT_CTRLL    160  // left of key
#define K_LEFT_CTRLR    161  // right of key
#define KT_FN           162
#define K_LEFT_SUPER    163
#define K_LEFT_ALT      164
#define K_SPACE1        165  // left of key
#define K_SPACE2        166  // left middle of key
#define K_SPACE3        168  // right middle of key
#define K_SPACE4        169  // right of key
#define K_RIGHT_ALT     170
#define K_APP           171
#define K_RIGHT_CTRLL   172  // left of key
#define K_RIGHT_CTRLR   173  // right of key
#define K_LEFT          174
#define K_DOWN          175
#define K_RIGHT         176
#define K_NUM_0         177
#define K_NUM_PERIOD    178
#define K_NUM_ENTERB    179  // bottom of key

#endif
