/*
  Special customized version for the DraStic emulator that runs on
  Miyoo Mini (Plus), TRIMUI-SMART and Miyoo A30 handhelds.

  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2022-2024 Steward Fu <steward.fu@gmail.com>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef __SDL_EVENT_MMIYOO_H__
#define __SDL_EVENT_MMIYOO_H__

#include "../../SDL_internal.h"


enum {
    MYKEY_UP = 0,
	MYKEY_DOWN,
    MYKEY_LEFT,
    MYKEY_RIGHT,
    MYKEY_A,
    MYKEY_B,
    MYKEY_X,
    MYKEY_Y,
    MYKEY_L1,
    MYKEY_R1,
    MYKEY_L2,
    MYKEY_R2,
    MYKEY_L3,
    MYKEY_R3,	
    MYKEY_SELECT,
    MYKEY_START,
    MYKEY_MENU,
    MYKEY_QSAVE,
    MYKEY_QLOAD,
    MYKEY_FF,
    MYKEY_EXIT,
    MYKEY_MENU_ONION,

	NUM_OF_MYKEY
};

#define MYKEY_LAST_BITS     21 // ignore POWER, VOL-, VOL+ keys



#define MMIYOO_KEYPAD_MODE 0
#define MMIYOO_MOUSE_MODE  1

typedef struct _MMIYOO_EventInfo {
    struct _keypad{
        uint32_t bitmaps;
    } keypad;

    struct _mouse{
        int x;
        int y;
        int maxx;
        int maxy;
    } mouse;

    int mode;
    
    // by trngaje
    int analogstick_left_x;
    int analogstick_left_y;
    int analogstick_right_x;
    int analogstick_right_y;
	int pen_display_cnt;
} MMIYOO_EventInfo;

extern void MMIYOO_EventInit(void);
extern void MMIYOO_EventDeinit(void);
extern void MMIYOO_PumpEvents(_THIS);

#endif

