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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <linux/input.h>

#include "../../SDL_internal.h"
#include "../../events/SDL_events_c.h"
#include "../../core/linux/SDL_evdev.h"
#include "../../thread/SDL_systhread.h"

#include "drastic_video.h"
#include "drastic_event.h"

    #define INPUT_DEV "/dev/input/event1" /* RG35XXH */
//    #define INPUT_DEV "/dev/input/event2" /* RG35XXH evmapy*/



/*
// for evmapy 
    #define UP      103
    #define DOWN    108
    #define LEFT    105
    #define RIGHT   106
    #define A       57
    #define B       29
    #define X       42
    #define Y       56
    #define L1      18
    #define L2      15
    #define R1      20
    #define R2      14
    #define START   28
    #define SELECT  97
    #define MENU    1
    #define POWER   116
    #define VOLUP   115
    #define VOLDOWN 114
	*/
    #define UP      -1	/* not defined */
    #define DOWN    -1 /* not defined */
    #define LEFT    -1 /* not defined */
    #define RIGHT   -1 /* not defined */
    #define A       BTN_SOUTH
    #define B       BTN_EAST
    #define X       BTN_NORTH
    #define Y       BTN_C
    #define L1      BTN_WEST
    #define L2      BTN_SELECT
    #define R1      BTN_Z
    #define R2      BTN_START
    #define START   BTN_TR
    #define SELECT  BTN_TL
    #define MENU    BTN_TL2
    #define POWER   116 /* not defined */
    #define VOLUP   115 /* not defined */
    #define VOLDOWN 114	/* not defined */
	#define L3		BTN_TR2
	#define R3		BTN_MODE


MMIYOO_EventInfo evt = {0};

extern GFX gfx;
extern NDS nds;
extern MMIYOO_VideoInfo vid;
extern int pixel_filter;

static int running = 0;
static int event_fd = -1;
static int lower_speed = 0;

static SDL_sem *event_sem = NULL;
static SDL_Thread *thread = NULL;
static uint32_t cur_keypad_bitmaps = 0;
static uint32_t pre_keypad_bitmaps = 0;

//extern int FB_W;
//extern int FB_H;
extern int g_lastX;
extern int g_lastY;

const SDL_Scancode code[]={
    SDLK_UP,            // UP
    SDLK_DOWN,          // DOWN
    SDLK_LEFT,          // LEFT
    SDLK_RIGHT,         // RIGHT
    SDLK_SPACE,         // A
    SDLK_LCTRL,         // B
    SDLK_LSHIFT,        // X
    SDLK_LALT,          // Y
    SDLK_e,             // L1
    SDLK_t,             // R1
    SDLK_TAB,           // L2
    SDLK_BACKSPACE,     // R2
	SDLK_4,	// L3
	SDLK_5, 	// R3
    SDLK_RCTRL,         // SELECT
    SDLK_RETURN,        // START
    SDLK_HOME,          // MENU
    SDLK_0,             // QUICK SAVE
    SDLK_1,             // QUICK LOAD
    SDLK_2,             // FAST FORWARD
    SDLK_3,             // EXIT
    SDLK_HOME,          // MENU (Onion system)
};

int volume_inc(void);
int volume_dec(void);

static void check_mouse_pos(void)
{
    if (evt.mouse.x < 0) {
        evt.mouse.x = 0;
    }
    if (evt.mouse.x >= evt.mouse.maxx) {
        evt.mouse.x = evt.mouse.maxx;
    }
    if (evt.mouse.y < 0) {
        evt.mouse.y = 0;
    }
    if (evt.mouse.y > evt.mouse.maxy) {
        evt.mouse.y = evt.mouse.maxy;
    }
}

static int get_move_interval(int type)
{
    float move = 0.0;
    long yv = nds.pen.yv;
    long xv = nds.pen.xv;

    if (lower_speed) {
        yv*= 2;
        xv*= 2;
    }

    if ((nds.dis_mode == NDS_DIS_MODE_HH0) || (nds.dis_mode == NDS_DIS_MODE_HH1)) {
        move = ((float)clock() - nds.pen.pre_ticks) / ((type == 0) ? yv : xv);
    }
    else {
        move = ((float)clock() - nds.pen.pre_ticks) / ((type == 0) ? xv : yv);
    }

    if (move <= 0.0) {
        move = 1.0;
    }
    return (int)(1.0 * move);
}

static void release_all_keys(void)
{
    int cc = 0;

    for (cc=0; cc<=MYKEY_LAST_BITS; cc++) {
        if (evt.keypad.bitmaps & 1) {
            SDL_SendKeyboardKey(SDL_RELEASED, SDL_GetScancodeFromKey(code[cc]));
        }
        evt.keypad.bitmaps>>= 1;
    }
}

static int hit_hotkey(uint32_t bit)
{
    uint32_t mask = (1 << bit) | (1 << ((nds.hotkey == HOTKEY_BIND_SELECT) ? MYKEY_SELECT : MYKEY_MENU));
    return (cur_keypad_bitmaps ^ mask) ? 0 : 1;
}

static void set_key(uint32_t bit, int val)
{
    if (val) {
        cur_keypad_bitmaps|= (1 << bit);
        evt.keypad.bitmaps|= (1 << bit);

        if (nds.hotkey == HOTKEY_BIND_SELECT) {
            if (bit == MYKEY_SELECT) {
                cur_keypad_bitmaps = (1 << MYKEY_SELECT);
            }
        }
        else {
            if (bit == MYKEY_MENU) {
                cur_keypad_bitmaps = (1 << MYKEY_MENU);
            }
        }

    }
    else {
        cur_keypad_bitmaps &= ~(1 << bit);
        evt.keypad.bitmaps &= ~(1 << bit);
    }
}

static int handle_hotkey(void)
{
    int hotkey_mask = 0;

    hotkey_mask = 1;
    if (nds.menu.enable || nds.menu.drastic.enable) {
        hotkey_mask = 0;
    }

    if (hotkey_mask && hit_hotkey(MYKEY_UP)) {
        if (evt.mode == MMIYOO_MOUSE_MODE) {
            switch (nds.dis_mode) {
            case NDS_DIS_MODE_VH_T0:
            case NDS_DIS_MODE_VH_T1:
            case NDS_DIS_MODE_S0:
            case NDS_DIS_MODE_S1:
                break;
            default:
                nds.pen.pos = 1;
                break;
            }
        }

        set_key(MYKEY_UP, 0);
    }

    if (hotkey_mask && hit_hotkey(MYKEY_DOWN)) {
        if (evt.mode == MMIYOO_MOUSE_MODE) {
            switch (nds.dis_mode) {
            case NDS_DIS_MODE_VH_T0:
            case NDS_DIS_MODE_VH_T1:
            case NDS_DIS_MODE_S0:
            case NDS_DIS_MODE_S1:
                break;
            default:
                nds.pen.pos = 0;
                break;
            }
        }

        set_key(MYKEY_DOWN, 0);
    }

    if (hotkey_mask && hit_hotkey(MYKEY_LEFT)) {
        if (nds.hres_mode == 0) {
            if (nds.dis_mode > 0) {
                nds.dis_mode -= 1;
            }
        }
        else {
            nds.dis_mode = NDS_DIS_MODE_HRES0;
        }


        set_key(MYKEY_LEFT, 0);
    }

    if (hotkey_mask && hit_hotkey(MYKEY_RIGHT)) {
        if (nds.hres_mode == 0) {
            if (nds.dis_mode < NDS_DIS_MODE_LAST) {
                nds.dis_mode += 1;
            }
        }
        else {
            nds.dis_mode = NDS_DIS_MODE_HRES1;
        }

        set_key(MYKEY_RIGHT, 0);
    }

    if (hotkey_mask && hit_hotkey(MYKEY_A)) {
        if ((evt.mode == MMIYOO_KEYPAD_MODE) && (nds.hres_mode == 0)) {
            uint32_t tmp = nds.alt_mode;
            nds.alt_mode = nds.dis_mode;
            nds.dis_mode = tmp;
        }

        set_key(MYKEY_A, 0);
    }

    if (hotkey_mask && hit_hotkey(MYKEY_B)) {
        pixel_filter = pixel_filter ? 0 : 1;
        set_key(MYKEY_B, 0);
    }

    if (hit_hotkey(MYKEY_X)) {
        set_key(MYKEY_X, 0);
    }

    if (hit_hotkey(MYKEY_Y)) {
        if (hotkey_mask) {
            if (evt.mode == MMIYOO_KEYPAD_MODE) {
                if ((nds.overlay.sel >= nds.overlay.max) &&
                    (nds.dis_mode != NDS_DIS_MODE_VH_T0) &&
                    (nds.dis_mode != NDS_DIS_MODE_VH_T1) &&
                    (nds.dis_mode != NDS_DIS_MODE_S1) &&
                    (nds.dis_mode != NDS_DIS_MODE_HRES1))
                {
                    nds.theme.sel+= 1;
                    if (nds.theme.sel > nds.theme.max) {
                        nds.theme.sel = 0;
                    }
                }
            }
            else {
                nds.pen.sel+= 1;
                if (nds.pen.sel >= nds.pen.max) {
                    nds.pen.sel = 0;
                }
                reload_pen();
            }
        }
        else {
            nds.menu.sel+= 1;
            if (nds.menu.sel >= nds.menu.max) {
                nds.menu.sel = 0;
            }
            reload_menu();

            if (nds.menu.drastic.enable) {
                SDL_SendKeyboardKey(SDL_PRESSED, SDLK_e);
                usleep(100000);
                SDL_SendKeyboardKey(SDL_RELEASED, SDLK_e);
            }
        }
        set_key(MYKEY_Y, 0);
    }

    if (hotkey_mask && hit_hotkey(MYKEY_START)) {
        if (nds.menu.enable == 0) {
            nds.menu.enable = 1;
            usleep(100000);
            handle_menu(-1);
            cur_keypad_bitmaps = 0;
            pre_keypad_bitmaps = evt.keypad.bitmaps = 0;
        }

        set_key(MYKEY_START, 0);
    }

    if (nds.hotkey == HOTKEY_BIND_MENU) {
        if (hotkey_mask && hit_hotkey(MYKEY_SELECT)) {
            set_key(MYKEY_MENU_ONION, 1);
            set_key(MYKEY_SELECT, 0);
        }
    }



    if (hotkey_mask && hit_hotkey(MYKEY_R1)) {
        static int pre_ff = 0;

        if (pre_ff != nds.fast_forward) {
            pre_ff = nds.fast_forward;
            //dtr_fastforward(nds.fast_forward);
        }
        set_key(MYKEY_FF, 1);

        set_key(MYKEY_R1, 0);
    }

    if (hotkey_mask && hit_hotkey(MYKEY_L1)) {
        set_key(MYKEY_EXIT, 1);

        set_key(MYKEY_L1, 0);
    }

    if (hotkey_mask && hit_hotkey(MYKEY_R2)) {
        set_key(MYKEY_QSAVE, 1);
        set_key(MYKEY_R2, 0);
    }

    if (hotkey_mask && hit_hotkey(MYKEY_L2)) {
        set_key(MYKEY_QSAVE, 1);

        set_key(MYKEY_L2, 0);
    }
    else if (evt.keypad.bitmaps & (1 << MYKEY_L2)) {
        if ((nds.menu.enable == 0) && (nds.menu.drastic.enable == 0)) {
            evt.mode = (evt.mode == MMIYOO_KEYPAD_MODE) ? MMIYOO_MOUSE_MODE : MMIYOO_KEYPAD_MODE;
            set_key(MYKEY_L2, 0);

            if (evt.mode == MMIYOO_MOUSE_MODE) {
                release_all_keys();
            }
            lower_speed = 0;
        }
    }

    if (!(evt.keypad.bitmaps & 0x0f)) {
        nds.pen.pre_ticks = clock();
    }

    return 0;
}

int EventUpdate(void *data)
{
    struct input_event ev = {0};

    uint32_t l1 = L1;
    uint32_t r1 = R1;
    uint32_t l2 = L2;
    uint32_t r2 = R2;

    uint32_t a = A;
    uint32_t b = B;
    uint32_t x = X;
    uint32_t y = Y;

    uint32_t up = UP;
    uint32_t down = DOWN;
    uint32_t left = LEFT;
    uint32_t right = RIGHT;

	int i;
	int input[NUM_OF_MYKEY] = {UP, DOWN, LEFT, RIGHT, A, B, X, Y, L1, R1, L2, R2, L3, R3,SELECT, START, MENU,-1, -1, -1, -1};

	for (i=0; i < NUM_OF_MYKEY; i++) {
		if (nds.input.key[i] != -1) {
			printf(PREFIX"nds.input.key[%d]=%d\n", i, nds.input.key[i] );
			input[i] = nds.input.key[i];
		}
	}
	

    while (running) {
        SDL_SemWait(event_sem);

        if ((nds.menu.enable == 0) && (nds.menu.drastic.enable == 0) && nds.keys_rotate) {
            if (nds.keys_rotate == 1) {
                up = input[MYKEY_LEFT];
                down = input[MYKEY_RIGHT];
                left = input[MYKEY_DOWN];
                right = input[MYKEY_UP];

                a = input[MYKEY_X];
                b = input[MYKEY_A];
                x = input[MYKEY_Y];
                y = input[MYKEY_B];
            }
            else {
                up = input[MYKEY_RIGHT];
                down = input[MYKEY_LEFT];
                left = input[MYKEY_UP];
                right = input[MYKEY_DOWN];

                a = input[MYKEY_B];
                b = input[MYKEY_Y];
                x = input[MYKEY_A];
                y = input[MYKEY_X];
            }
        }
        else {
            up = input[MYKEY_UP];
            down = input[MYKEY_DOWN];
            left = input[MYKEY_LEFT];
            right = input[MYKEY_RIGHT];

            a = input[MYKEY_A];
            b = input[MYKEY_B];
            x = input[MYKEY_X];
            y = input[MYKEY_Y];
        }


        if (nds.swap_l1l2) {
            l1 = input[MYKEY_L2];
            l2 = input[MYKEY_L1];
        }
        else {
            l1 = input[MYKEY_L1];
            l2 = input[MYKEY_L2];
        }

        if (nds.swap_r1r2) {
            r1 = input[MYKEY_R2];
            r2 = input[MYKEY_R1];
        }
        else {
            r1 = input[MYKEY_R1];
            r2 = input[MYKEY_R2];
        }


        if (event_fd > 0) {
            int r = 0;

            if (read(event_fd, &ev, sizeof(struct input_event))) {
				if (ev.type == EV_ABS) {
					if (ev.code == ABS_HAT0X) {
						 if ((nds.menu.enable == 0) && (nds.menu.drastic.enable == 0) && nds.keys_rotate) {
							if (nds.keys_rotate == 1) {
								// degree 270
								if (ev.value < 0) { 
									// up 
									if (evt.keypad.bitmaps & (1 << MYKEY_DOWN)) { 
										set_key(MYKEY_DOWN, 0); 
									} 
									set_key(MYKEY_UP,    1); 
								}  
								else if (ev.value > 0) { 
									// down 
									if (evt.keypad.bitmaps & (1 << MYKEY_UP)) {
										set_key(MYKEY_UP, 0); 
									} 
									set_key(MYKEY_DOWN,    1); 
								} 
								else { 
									// center 
									if (evt.keypad.bitmaps & (1 << MYKEY_UP)) { 
										set_key(MYKEY_UP, 0); 
									} 

									if (evt.keypad.bitmaps & (1 << MYKEY_DOWN)) { 
										set_key(MYKEY_DOWN, 0); 
									} 
								}
							}
							else {
								// degree 90
								if (ev.value > 0) { 
									// up 
									if (evt.keypad.bitmaps & (1 << MYKEY_DOWN)) { 
										set_key(MYKEY_DOWN, 0); 
									} 
									set_key(MYKEY_UP,    1); 
								}  
								else if (ev.value < 0) { 
									// down 
									if (evt.keypad.bitmaps & (1 << MYKEY_UP)) {
										set_key(MYKEY_UP, 0); 
									} 
									set_key(MYKEY_DOWN,    1); 
								} 
								else { 
									// center 
									if (evt.keypad.bitmaps & (1 << MYKEY_UP)) { 
										set_key(MYKEY_UP, 0); 
									} 

									if (evt.keypad.bitmaps & (1 << MYKEY_DOWN)) { 
										set_key(MYKEY_DOWN, 0); 
									} 
								} 
							}
						 }
						else {
							// degree 0
				
							if (ev.value < 0) {
								// left
								if (evt.keypad.bitmaps & (1 << MYKEY_RIGHT)) {
									set_key(MYKEY_RIGHT, 0);
								}
								set_key(MYKEY_LEFT,    1); 
							} 
							else if (ev.value > 0) {
								// right
								if (evt.keypad.bitmaps & (1 << MYKEY_LEFT)) {
									set_key(MYKEY_LEFT, 0);
								} 
								set_key(MYKEY_RIGHT,    1); 
							} 
							else { 
								// center 
								if (evt.keypad.bitmaps & (1 << MYKEY_LEFT)) { 
									set_key(MYKEY_LEFT, 0); 
								} 
								if (evt.keypad.bitmaps & (1 << MYKEY_RIGHT)) { 
									set_key(MYKEY_RIGHT, 0); 
								} 
							} 
						}
					} 
					else if (ev.code == ABS_HAT0Y) { 
						if ((nds.menu.enable == 0) && (nds.menu.drastic.enable == 0) && nds.keys_rotate) {
							if (nds.keys_rotate == 1) {
								// degree 270
								if (ev.value > 0) {
									// left
									if (evt.keypad.bitmaps & (1 << MYKEY_RIGHT)) {
										set_key(MYKEY_RIGHT, 0);
									}
									set_key(MYKEY_LEFT,    1); 
								} 
								else if (ev.value < 0) {
									// right
									if (evt.keypad.bitmaps & (1 << MYKEY_LEFT)) {
										set_key(MYKEY_LEFT, 0);
									} 
									set_key(MYKEY_RIGHT,    1); 
								} 
								else { 
									// center 
									if (evt.keypad.bitmaps & (1 << MYKEY_LEFT)) { 
										set_key(MYKEY_LEFT, 0); 
									} 
									if (evt.keypad.bitmaps & (1 << MYKEY_RIGHT)) { 
										set_key(MYKEY_RIGHT, 0); 
									} 
								} 
							}
							else {
								// degree 90
								if (ev.value < 0) {
									// left
									if (evt.keypad.bitmaps & (1 << MYKEY_RIGHT)) {
										set_key(MYKEY_RIGHT, 0);
									}
									set_key(MYKEY_LEFT,    1); 
								} 
								else if (ev.value > 0) {
									// right
									if (evt.keypad.bitmaps & (1 << MYKEY_LEFT)) {
										set_key(MYKEY_LEFT, 0);
									} 
									set_key(MYKEY_RIGHT,    1); 
								} 
								else { 
									// center 
									if (evt.keypad.bitmaps & (1 << MYKEY_LEFT)) { 
										set_key(MYKEY_LEFT, 0); 
									} 
									if (evt.keypad.bitmaps & (1 << MYKEY_RIGHT)) { 
										set_key(MYKEY_RIGHT, 0); 
									} 
								} 
							}
						}
						else {
							// degree 0
							
							if (ev.value < 0) { 
								// up 
								if (evt.keypad.bitmaps & (1 << MYKEY_DOWN)) { 
									set_key(MYKEY_DOWN, 0); 
								} 
								set_key(MYKEY_UP,    1); 
							}  
							else if (ev.value > 0) { 
								// down 
								if (evt.keypad.bitmaps & (1 << MYKEY_UP)) {
									set_key(MYKEY_UP, 0); 
								} 
								set_key(MYKEY_DOWN,    1); 
							} 
							else { 
								// center 
								if (evt.keypad.bitmaps & (1 << MYKEY_UP)) { 
									set_key(MYKEY_UP, 0); 
								} 

								if (evt.keypad.bitmaps & (1 << MYKEY_DOWN)) { 
									set_key(MYKEY_DOWN, 0); 
								} 
							} 
						}
					} 
					else if (ev.code == ABS_Z) {
						// left analog x
						if (ev.value < -100) {
							// left
							evt.analogstick_left_x = ev.value / 1024;;
						}
						else if (ev.value > 100) {
							// right
							evt.analogstick_left_x = ev.value / 1024;;
						}
						else {
							// center 
							evt.analogstick_left_x = 0;
						}						
					}
					else if (ev.code == ABS_RX) {
						// left analog y
						if (ev.value < -100) {
							// up
							evt.analogstick_left_y = ev.value / 1024;
						}
						else if (ev.value > 100) {
							// down
							evt.analogstick_left_y = ev.value / 1024;
						}
						else {
							// center 
							evt.analogstick_left_y = 0;
						}		
					}					
					else if (ev.code == ABS_RY) {
						if (nds.dis_mode == NDS_DIS_MODE_HH1) {
							if (evt.mode == MMIYOO_KEYPAD_MODE) {
								// right analog x -> dpad
								// left -> up
								// right -> down
								if (ev.value < -100) {
									// up 
									if (evt.keypad.bitmaps & (1 << MYKEY_DOWN)) { 
										set_key(MYKEY_DOWN, 0); 
									} 
									set_key(MYKEY_UP,    1); 							
								}
								else if (ev.value > 100) {
									// down 
									if (evt.keypad.bitmaps & (1 << MYKEY_UP)) {
										set_key(MYKEY_UP, 0); 
									} 
									set_key(MYKEY_DOWN,    1); 							
								}
								else {
									// center 
									if (evt.keypad.bitmaps & (1 << MYKEY_UP)) { 
										set_key(MYKEY_UP, 0); 
									} 

									if (evt.keypad.bitmaps & (1 << MYKEY_DOWN)) { 
										set_key(MYKEY_DOWN, 0); 
									} 							
								}
							}
							else {
								// right analog x -> touch
								// left -> up
								// right -> down
								if (ev.value < -100) {
									// up 
									evt.analogstick_right_y = ev.value / 1024;
								}
								else if (ev.value > 100) {
									// down 
									evt.analogstick_right_y= ev.value / 1024;
								}
								else {
									// center 
									evt.analogstick_right_y = 0;
								}								
							}
						}
					}
					else if (ev.code ==  ABS_RZ) {
						if (nds.dis_mode == NDS_DIS_MODE_HH1) {
							if (evt.mode == MMIYOO_KEYPAD_MODE) {
								// right analog y to dpad
								// down ->left
								// up -> right
								if (ev.value < -100) {
									// right
									if (evt.keypad.bitmaps & (1 << MYKEY_LEFT)) {
										set_key(MYKEY_LEFT, 0);
									} 
									set_key(MYKEY_RIGHT,    1); 					
								}
								else if (ev.value > 100) {
									// left
									if (evt.keypad.bitmaps & (1 << MYKEY_RIGHT)) {
										set_key(MYKEY_RIGHT, 0);
									}
									set_key(MYKEY_LEFT,    1); 									
								}
								else {
									// center 
									if (evt.keypad.bitmaps & (1 << MYKEY_LEFT)) { 
										set_key(MYKEY_LEFT, 0); 
									} 
									if (evt.keypad.bitmaps & (1 << MYKEY_RIGHT)) { 
										set_key(MYKEY_RIGHT, 0); 
									} 							
								}
							}
							else {
								// right analog y to touch
								// down ->left
								// up -> right
								if (ev.value < -100) {
									// up 
									evt.analogstick_right_x= ev.value  *  (-1) / 1024;
								}
								else if (ev.value > 100) {
									// down 
									evt.analogstick_right_x= ev.value  * (-1) / 1024;
								}
								else {
									// center 
									evt.analogstick_right_x  = 0;
								}										
							}
						}
					}
				}
                else if ((ev.type == EV_KEY) && (ev.value != 2)) {
                    r = 1;
                    //printf(PREFIX"code:%d, value:%d\n", ev.code, ev.value);
                    if (ev.code == l1)      { set_key(MYKEY_L1,    ev.value); }
                    if (ev.code == r1)      { set_key(MYKEY_R1,    ev.value); }

                    if (up != -1 && ev.code == up)      { set_key(MYKEY_UP,    ev.value); }
                    if (down != -1 && ev.code == down)    { set_key(MYKEY_DOWN,  ev.value); }
                    if (left != -1 && ev.code == left)    { set_key(MYKEY_LEFT,  ev.value); }
                    if (right != -1 && ev.code == right)   { set_key(MYKEY_RIGHT, ev.value); }

                    if (ev.code == a)       { set_key(MYKEY_A,     ev.value); }
                    if (ev.code == b)       { set_key(MYKEY_B,     ev.value); }
                    if (ev.code == x)       { set_key(MYKEY_X,     ev.value); }
                    if (ev.code == y)       { set_key(MYKEY_Y,     ev.value); }


                    if (ev.code == r2)      { set_key(MYKEY_R2,    ev.value); }
                    if (ev.code == l2)      { set_key(MYKEY_L2,    ev.value); }


                    if (ev.code == input[MYKEY_L3])      { set_key(MYKEY_L3,    ev.value);}
                    if (ev.code == input[MYKEY_R3])      { set_key(MYKEY_R3,    ev.value);}
					if (input[MYKEY_QSAVE] != -1 && ev.code == input[MYKEY_QSAVE])      { set_key(MYKEY_QSAVE,    ev.value); }
					if (input[MYKEY_QLOAD] != -1 && ev.code == input[MYKEY_QLOAD])      { set_key(MYKEY_QLOAD,    ev.value); }
					if (input[MYKEY_EXIT] != -1 && ev.code == input[MYKEY_EXIT])      { set_key(MYKEY_EXIT,    ev.value); }
	
                    if (ev.code == input[MYKEY_START])      { set_key(MYKEY_START,    ev.value); }
                    if (ev.code == input[MYKEY_SELECT])      { set_key(MYKEY_SELECT,    ev.value); }
                    if (ev.code == input[MYKEY_MENU])      { set_key(MYKEY_MENU,    ev.value); }					
					
                }
            }

            handle_hotkey();
        }
        SDL_SemPost(event_sem);
        usleep(1000000 / 60);
    }
    
    return 0;
}

void MMIYOO_EventInit(void)
{
    DIR *dir = NULL;

    pre_keypad_bitmaps = 0;
    memset(&evt, 0, sizeof(evt));
    evt.mouse.maxx = NDS_W;
    evt.mouse.maxy = NDS_H;
    evt.mouse.x = evt.mouse.maxx >> 1;
    evt.mouse.y = evt.mouse.maxy >> 1;

    evt.mode = MMIYOO_KEYPAD_MODE;

	if (strlen(nds.input.dev) > 0) {
		printf(PREFIX"nds.input.dev=%s\n", nds.input.dev);
		event_fd = open(nds.input.dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	else {
		event_fd = open(INPUT_DEV, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	
    if(event_fd < 0){
        printf(PREFIX"Failed to open event0\n");
    }

    if((event_sem =  SDL_CreateSemaphore(1)) == NULL) {
        SDL_SetError("Can't create input semaphore");
        return;
    }

    running = 1;
    if((thread = SDL_CreateThreadInternal(EventUpdate, "MMIYOOInputThread", 4096, NULL)) == NULL) {
        SDL_SetError("Can't create input thread");
        return;
    }
}

void MMIYOO_EventDeinit(void)
{
    running = 0;
    SDL_WaitThread(thread, NULL);
    SDL_DestroySemaphore(event_sem);
    if(event_fd > 0) {
        close(event_fd);
        event_fd = -1;
    }

}

void MMIYOO_PumpEvents(_THIS)
{
    SDL_SemWait(event_sem);
    if (nds.menu.enable) {
        int cc = 0;
        uint32_t bit = 0;
        uint32_t changed = pre_keypad_bitmaps ^ evt.keypad.bitmaps;

        for (cc=0; cc<=MYKEY_LAST_BITS; cc++) {
            bit = 1 << cc;
            if (changed & bit) {
                if ((evt.keypad.bitmaps & bit) == 0) {
                    handle_menu(cc);
                }
            }
        }
        pre_keypad_bitmaps = evt.keypad.bitmaps;
    }
    else {
        if (evt.mode == MMIYOO_KEYPAD_MODE) {
            if (pre_keypad_bitmaps != evt.keypad.bitmaps) {
                int cc = 0;
                uint32_t bit = 0;
                uint32_t changed = pre_keypad_bitmaps ^ evt.keypad.bitmaps;

                for (cc=0; cc<=MYKEY_LAST_BITS; cc++) {
                    bit = 1 << cc;

                    if ((nds.hotkey == HOTKEY_BIND_MENU) && (cc == MYKEY_MENU)) {
                        continue;
                    }

                    if (changed & bit) {
                        SDL_SendKeyboardKey((evt.keypad.bitmaps & bit) ? SDL_PRESSED : SDL_RELEASED, SDL_GetScancodeFromKey(code[cc]));
                    }
                }
				
				if (changed & (1 << MYKEY_R3)) {
                    SDL_SendMouseButton(vid.window, 0, (evt.keypad.bitmaps & (1 << MYKEY_R3)) ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_LEFT);
                }

                if (pre_keypad_bitmaps & (1 << MYKEY_QSAVE)) {
                    nds.state|= NDS_STATE_QSAVE;
                    set_key(MYKEY_QSAVE, 0);
                }
                if (pre_keypad_bitmaps & (1 << MYKEY_QLOAD)) {
                    nds.state|= NDS_STATE_QLOAD;
                    set_key(MYKEY_QLOAD, 0);
                }
                if (pre_keypad_bitmaps & (1 << MYKEY_FF)) {
                    nds.state|= NDS_STATE_FF;
                    set_key(MYKEY_FF, 0);
                }
                if (pre_keypad_bitmaps & (1 << MYKEY_MENU_ONION)) {
                    set_key(MYKEY_MENU_ONION, 0);
                }
                if (pre_keypad_bitmaps & (1 << MYKEY_EXIT)) {
                    release_all_keys();
                }
                pre_keypad_bitmaps = evt.keypad.bitmaps;
            }
        }
        else {
            int updated = 0;
            
            if (pre_keypad_bitmaps != evt.keypad.bitmaps) {
                uint32_t cc = 0;
                uint32_t bit = 0;
                uint32_t changed = pre_keypad_bitmaps ^ evt.keypad.bitmaps;

                if (changed & (1 << MYKEY_A)) {
                    SDL_SendMouseButton(vid.window, 0, (evt.keypad.bitmaps & (1 << MYKEY_A)) ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_LEFT);
                }

				if (changed & (1 << MYKEY_R3)) {
                    SDL_SendMouseButton(vid.window, 0, (evt.keypad.bitmaps & (1 << MYKEY_R3)) ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_LEFT);
                }

                for (cc=0; cc<=MYKEY_LAST_BITS; cc++) {
                    bit = 1 << cc;
                    if ((cc == MYKEY_FF) || (cc == MYKEY_QSAVE) || (cc == MYKEY_QLOAD) || (cc == MYKEY_EXIT) || (cc == MYKEY_R2)) {
                        if (changed & bit) {
                            SDL_SendKeyboardKey((evt.keypad.bitmaps & bit) ? SDL_PRESSED : SDL_RELEASED, SDL_GetScancodeFromKey(code[cc]));
                        }
                    }
                    if (cc == MYKEY_R1) {
                        if (changed & bit) {
                            lower_speed = (evt.keypad.bitmaps & bit);
                        }
                    }
                }
            }

            if (((nds.dis_mode == NDS_DIS_MODE_HH0) || (nds.dis_mode == NDS_DIS_MODE_HH1)) && (nds.keys_rotate == 0)) {
                if (evt.keypad.bitmaps & (1 << MYKEY_UP)) {
                    updated = 1;
                    evt.mouse.x+= get_move_interval(1);
                }
                if (evt.keypad.bitmaps & (1 << MYKEY_DOWN)) {
                    updated = 1;
                    evt.mouse.x-= get_move_interval(1);
                }
                if (evt.keypad.bitmaps & (1 << MYKEY_LEFT)) {
                    updated = 1;
                    evt.mouse.y-= get_move_interval(0);
                }
                if (evt.keypad.bitmaps & (1 << MYKEY_RIGHT)) {
                    updated = 1;
                    evt.mouse.y+= get_move_interval(0);
                }
            }
            else {
                if (evt.keypad.bitmaps & (1 << MYKEY_UP)) {
                    updated = 1;
                    evt.mouse.y-= get_move_interval(1);
                }
                if (evt.keypad.bitmaps & (1 << MYKEY_DOWN)) {
                    updated = 1;
                    evt.mouse.y+= get_move_interval(1);
                }
                if (evt.keypad.bitmaps & (1 << MYKEY_LEFT)) {
                    updated = 1;
                    evt.mouse.x-= get_move_interval(0);
                }
                if (evt.keypad.bitmaps & (1 << MYKEY_RIGHT)) {
                    updated = 1;
                    evt.mouse.x+= get_move_interval(0);
                }
            }
            check_mouse_pos();

            if(updated){
                int x = 0;
                int y = 0;

                x = (evt.mouse.x * 320) / evt.mouse.maxx;
                y = (evt.mouse.y * 240) / evt.mouse.maxy;
               SDL_SendMouseMotion(vid.window, 0, 0, x + 160, y + (nds.pen.pos ? 240 : 0));
            }

            if (pre_keypad_bitmaps & (1 << MYKEY_QSAVE)) {
                set_key(MYKEY_QSAVE, 0);
            }
            if (pre_keypad_bitmaps & (1 << MYKEY_QLOAD)) {
                set_key(MYKEY_QLOAD, 0);
            }
            if (pre_keypad_bitmaps & (1 << MYKEY_FF)) {
                set_key(MYKEY_FF, 0);
            }
            if (pre_keypad_bitmaps & (1 << MYKEY_EXIT)) {
                release_all_keys();
            }
            pre_keypad_bitmaps = evt.keypad.bitmaps;
        }
    }

	{
		// check left analog stick	
		 int updated = 0;
		 if (evt.analogstick_left_y != 0) {
			updated = 1;
			evt.mouse.y+= evt.analogstick_left_y;
			evt.pen_display_cnt = 180;
		}

		if (evt.analogstick_left_x != 0) {
			updated = 1;
			evt.mouse.x+= evt.analogstick_left_x;
			evt.pen_display_cnt = 180;
		}

		// check right analog stick	
		 if (evt.analogstick_right_y != 0) {
			updated = 1;
			evt.mouse.y+= evt.analogstick_right_y;
			evt.pen_display_cnt = 180;
		}

		if (evt.analogstick_right_x != 0) {
			updated = 1;
			evt.mouse.x+= evt.analogstick_right_x;
			evt.pen_display_cnt = 180;
		}
		
		check_mouse_pos();

		if(updated){
			int x = 0;
			int y = 0;

			x = (evt.mouse.x * 320) / evt.mouse.maxx;
			y = (evt.mouse.y * 240) / evt.mouse.maxy;
		   SDL_SendMouseMotion(vid.window, 0, 0, x + 160, y + (nds.pen.pos ? 240 : 0));
		}	
	}

    SDL_SemPost(event_sem);
}

#ifdef UNITTEST
#include "unity_fixture.h"

TEST_GROUP(sdl2_event_mmiyoo);

TEST_SETUP(sdl2_event_mmiyoo)
{
}

TEST_TEAR_DOWN(sdl2_event_mmiyoo)
{
}

TEST(sdl2_event_mmiyoo, hit_hotkey)
{
    TEST_ASSERT_EQUAL(hit_hotkey(0), 0);
}

TEST_GROUP_RUNNER(sdl2_event_mmiyoo)
{
    RUN_TEST_CASE(sdl2_event_mmiyoo, hit_hotkey);
}
#endif

