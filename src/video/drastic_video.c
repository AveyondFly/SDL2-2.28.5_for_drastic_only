/*
  Special customized version for the DraStic emulator that runs on
  rgxxxx, trimui handhelds.

  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2022-2024 Steward Fu <steward.fu@gmail.com>
  Copyright (C) 2024 trngaje <trngaje@gmail.com>

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

#include <time.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <json-c/json.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

//#define ADVDRASTIC_DRM

#ifdef ADVDRASTIC_DRM
#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#endif

#include <errno.h>

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include <SDL2/SDL_image.h>
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "drastic_video.h"
#include "drastic_event.h"

#include "drastic_hex_pen.h"
#include "drastic_bios_arm7.h"
#include "drastic_bios_arm9.h"
#include "drastic_nds_bios_arm7.h"
#include "drastic_nds_bios_arm9.h"
#include "drastic_nds_firmware.h"

#include "jsonlayout.h"

NDS nds = {0};
GFX gfx = {0};
MMIYOO_VideoInfo vid = {0};

// add by trngaje
ADVANCE_CONTROLS g_advControls[NUM_OF_ADVANCE_CONTROL_INDEX] = {
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},   
    
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff},
    {0xffff,0xffff}, 

    {0xffff,0xffff}    
    };

/*
    unsigned long ulInput;
    unsigned int uiUseSAVformat;
    unsigned int uiRotateDPAD;
    unsigned int uiRotateButtons;
    unsigned int uiEnableautosavestate;
    unsigned int uiSlotOfAutosavestate;
*/
ADVANCE_DRASTIC      g_advdrastic = {0, 0, 0, 0, 0, 10,}; // uiSlotOfAutosavestate = 10 default
extern theme_t g_theme;

//int FB_W = 0;
//int FB_H = 0;
//int FB_SIZE = 0;
int LINE_H = 0;
//int TMP_SIZE = 0;
int FONT_SIZE = 0;
int show_fps = 0;
int pixel_filter = 1;
int savestate_busy = 0;
SDL_Surface *fps_info = NULL;

// add by trngaje begin ---------------------------------------------------------
unsigned int g_save_slot = 0;

#ifdef ADVDRASTIC_DRM
struct drm_device {
	uint32_t width;			//显示器的宽的像素点数量
	uint32_t height;		//显示器的高的像素点数量
	uint32_t pitch;			//每行占据的字节数
	uint32_t handle;		//drm_mode_create_dumb的返回句柄
	uint32_t size;			//显示器占据的总字节数
	uint32_t *vaddr;		//mmap的首地址
	uint32_t fb_id;			//创建的framebuffer的id号
    	struct gbm_device *gbm;
    	struct gbm_surface *gbm_surface;
	struct gbm_bo *previous_bo;
	struct drm_mode_create_dumb create ;	//创建的dumb
    struct drm_mode_map_dumb map;			//内存映射结构体
};

drmModeConnector *conn;	    //connetor相关的结构体
drmModeRes *res;		    //资源
uint32_t conn_id;           //connetor的id号
uint32_t crtc_id;           //crtc的id号
int dri_fd;					    //文件描述符

#define RED 0XFF0000
#define GREEN 0X00FF00
#define BLUE 0X0000FF

struct drm_device drm_buf;
#endif

// Declaration of thread condition variable
pthread_cond_t request_update_screen_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t response_update_screen_cond = PTHREAD_COND_INITIALIZER;

// declaring mutex
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
// add by trngaje end ---------------------------------------------------------


static pthread_t thread;
static volatile int is_running = 0;
static int need_reload_bg = RELOAD_BG_COUNT;
static SDL_Surface *cvt = NULL;
static SDL_GLContext sdl_gl_context = NULL;
static int egl_initialized = 0;
static SDL_Window *found_window = NULL;

//extern scale_mat_NEON(const Uint32 *src, int src_w, int src_h, int src_pitch, Uint32 *dst, int dst_w, int dst_h, int dst_pitch);
extern void scale_mat(const Uint32 *src, int src_w, int src_h, int src_pitch,
          Uint32 *dst, int dst_w, int dst_h, int dst_pitch);
		  
extern MMIYOO_EventInfo evt;

static int MMIYOO_VideoInit(_THIS);
static int MMIYOO_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void MMIYOO_VideoQuit(_THIS);

static CUST_MENU drastic_menu = {0};
static char *translate[MAX_LANG_LINE] = {0};

GLfloat bgVertices[] = {
   -1.0f,  1.0f,  0.0f,  0.0f,  0.0f,
   -1.0f, -1.0f,  0.0f,  0.0f,  1.0f,
    1.0f, -1.0f,  0.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  0.0f,  1.0f,  0.0f
};

GLfloat vVertices[] = {
   -1.0f,  1.0f,  0.0f,  0.0f,  0.0f,
   -1.0f, -1.0f,  0.0f,  0.0f,  1.0f,
    1.0f, -1.0f,  0.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  0.0f,  1.0f,  0.0f
};
GLushort indices[] = {0, 1, 2, 0, 2, 3};

const char *vShaderSrc =
    "attribute vec4 a_position;   \n"
    "attribute vec2 a_texCoord;   \n"
    "varying vec2 v_texCoord;     \n"
    "void main()                  \n"
    "{                            \n"
    "   gl_Position = a_position; \n"
    "   v_texCoord = a_texCoord;  \n"
    "}                            \n";
	
const char *vShaderDegree90Src =
    "attribute vec4 a_position;   \n"
    "attribute vec2 a_texCoord;   \n"
    "attribute float a_alpha;     \n"
    "varying vec2 v_texCoord;     \n"
    "void main()                  \n"
    "{                            \n"
    "    const float angle = 90.0 * (3.1415 * 2.0) / 360.0;                                                                            \n"
    "    mat4 rot = mat4(cos(angle), -sin(angle), 0.0, 0.0, sin(angle), cos(angle), 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0); \n"
    "    gl_Position = a_position * rot; \n"
    "    v_texCoord = a_texCoord;        \n"
    "}                                   \n";
	
const char *vShaderDegree180Src =
    "attribute vec4 a_position;   \n"
    "attribute vec2 a_texCoord;   \n"
    "attribute float a_alpha;     \n"
    "varying vec2 v_texCoord;     \n"
    "void main()                  \n"
    "{                            \n"
    "    const float angle = 180.0 * (3.1415 * 2.0) / 360.0;                                                                            \n"
    "    mat4 rot = mat4(cos(angle), -sin(angle), 0.0, 0.0, sin(angle), cos(angle), 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0); \n"
    "    gl_Position = a_position * rot; \n"
    "    v_texCoord = a_texCoord;        \n"
    "}                                   \n";
	
const char *vShaderDegree270Src =
    "attribute vec4 a_position;   \n"
    "attribute vec2 a_texCoord;   \n"
    "attribute float a_alpha;     \n"
    "varying vec2 v_texCoord;     \n"
    "void main()                  \n"
    "{                            \n"
    "    const float angle = 270.0 * (3.1415 * 2.0) / 360.0;                                                                            \n"
    "    mat4 rot = mat4(cos(angle), -sin(angle), 0.0, 0.0, sin(angle), cos(angle), 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0); \n"
    "    gl_Position = a_position * rot; \n"
    "    v_texCoord = a_texCoord;        \n"
    "}                                   \n";

    
const char *fShaderSrc =
    "precision mediump float;                                  \n"
    "varying vec2 v_texCoord;                                  \n"
    "uniform float s_alpha;                                    \n"
    "uniform sampler2D s_texture;                              \n"
    "void main()                                               \n"
    "{                                                         \n"
    "    if (s_alpha >= 2.0) {                                 \n"
    "        gl_FragColor = texture2D(s_texture, v_texCoord);  \n"
    "    }                                                     \n"
    "    else if (s_alpha > 0.0) {                             \n"
    "        vec3 tex = texture2D(s_texture, v_texCoord).bgr;  \n"
    "        gl_FragColor = vec4(tex, s_alpha);                \n"
    "    }                                                     \n"
    "    else {                                                \n"
    "        vec3 tex = texture2D(s_texture, v_texCoord).bgr;  \n"
    "        gl_FragColor = vec4(tex, 1.0);                    \n"
    "    }                                                     \n"
    "}                                                         \n";


static void write_file(const char *fname, const void *buf, int len)
{
    struct stat st = {0};
    int fd = -1;

    if (stat(buf, &st) == -1) {
        fd = open(fname, O_WRONLY | O_CREAT, 0755);
    }
    else {
        fd = open(fname, O_WRONLY);
    }

    if (fd >= 0) {
        write(fd, buf, len);
        close(fd);
    }
}

static int get_current_menu_layer(void)
{
    int cc = 0;
    const char *P0 = "Change Options";
    const char *P1 = "Frame skip type";
    const char *P2 = "D-Pad Up";
    const char *P3 = "Enter Menu";
    const char *P4 = "Username";
    const char *P5 = "KB Space: toggle cheat/folder    KB Left Ctrl: return to main menu";
    const char *P6 = "KB Space: select";

    for (cc=0; cc<drastic_menu.cnt; cc++) {
        printf("[trngaje] get_current_menu_layer:%d,%d,%s\n", cc, drastic_menu.item[cc].x, drastic_menu.item[cc].msg);
        if ((drastic_menu.item[cc].x == 180) && !memcmp(drastic_menu.item[cc].msg, P0, strlen(P0))) {
            return NDS_DRASTIC_MENU_MAIN;
        }
        else if ((drastic_menu.item[cc].x == 92) && !memcmp(drastic_menu.item[cc].msg, P1, strlen(P1))) {
            return NDS_DRASTIC_MENU_OPTION;
        }
        else if ((drastic_menu.item[cc].x == 56) && !memcmp(drastic_menu.item[cc].msg, P2, strlen(P2))) {
            return NDS_DRASTIC_MENU_CONTROLLER;
        }
        else if ((drastic_menu.item[cc].x == 57/*56*/) && !memcmp(drastic_menu.item[cc].msg, P3, strlen(P3))) {
            return NDS_DRASTIC_MENU_CONTROLLER2;
        }
        else if ((drastic_menu.item[cc].x == 92) && !memcmp(drastic_menu.item[cc].msg, P4, strlen(P4))) {
            return NDS_DRASTIC_MENU_FIRMWARE;
        }
        else if ((drastic_menu.item[cc].x == 6) && !memcmp(drastic_menu.item[cc].msg, P5, strlen(P5))) {
            return NDS_DRASTIC_MENU_CHEAT;
        }
        else if ((drastic_menu.item[cc].x == 6) && !memcmp(drastic_menu.item[cc].msg, P6, strlen(P6))) {
            return NDS_DRASTIC_MENU_ROM;
        }
    }
    return -1;
}

/*
[trngaje] cc=0, x=352, y=201, Version r2.5.2.0
[trngaje] cc=1, x=544, y=233, (No savestate)
[trngaje] cc=2, x=180, y=280, Change Options
[trngaje] cc=3, x=180, y=288, Change Steward Options
[trngaje] cc=4, x=180, y=296, Configure Controls
[trngaje] cc=5, x=180, y=304, Configure Firmware
[trngaje] cc=6, x=180, y=312, Configure Cheats
[trngaje] cc=7, x=180, y=320, Load state   0
[trngaje] cc=8, x=180, y=328, Save state   0
[trngaje] cc=9, x=180, y=344, Load new game
[trngaje] cc=10, x=180, y=352, Restart game
[trngaje] cc=11, x=180, y=368, Return to game
[trngaje] cc=12, x=180, y=384, Exit DraStic-trngaje
*/
static int draw_drastic_menu_main(void)
{
    int cc = 0;
    //int div = 1;
    //int w = 30;
    //int h = 100;
    int w = g_advdrastic.iDisplay_width * 30 / 640;
    int h = g_advdrastic.iDisplay_height * 100 / 480;
    int draw = 0;
    int draw_shot = 0;
    int x = 0, y = 0;
    SDL_Rect rt = {0};
    CUST_MENU_SUB *p = NULL;
    char buf[MAX_PATH << 1] = {0};
    int menu_line_cnt;
    uint32_t bgcolor;

    menu_line_cnt=0;
    for (cc=0; cc<drastic_menu.cnt; cc++) {
        draw = 0;
        //x = 90;// / div;
        x = g_advdrastic.iDisplay_width * 90 / 640;
        w = LINE_H; // / div;
        //h = nds.enable_752x560 ? (115 / div) : (100 / div);
        //h = 100;
        h = g_advdrastic.iDisplay_height * 100 / 480;

        memset(buf, 0, sizeof(buf));
        p = &drastic_menu.item[cc];
        
        //printf("[trngaje] cc=%d, x=%d, y=%d, %s\n", cc, p->x, p->y, drastic_menu.item[cc].msg);
        if (p->y == 201) {
            draw = 1;

            sprintf(buf, "NDS %s", &p->msg[8]);

            x = g_advdrastic.iDisplay_width - get_font_width(buf) - 10;
            //y = 10 / div;
            //y = 10;
            y = g_advdrastic.iDisplay_height * 10 / 480;
        }
        else if (p->x == 180) {
            draw = 1;
            y = h + ((menu_line_cnt) * w);
            menu_line_cnt++;
            strcpy(buf, to_lang(drastic_menu.item[cc].msg));             
        }
        else if (p->x ==  544) {
            draw = 1;
            //x = 320;
            x = g_advdrastic.iDisplay_width * 0.5; 
            //y = 480-(LINE_H*2);
            y = g_advdrastic.iDisplay_height - (LINE_H*2);
            strcpy(buf, to_lang(drastic_menu.item[cc].msg));                  
        }
 
        if (draw) {
            if (p->bg) {
                
                //rt.x = 5 / div;
                //rt.y = y - (3 / div);
                //rt.w = g_advdrastic.iDisplay_width - (10 / div);
                //rt.x = 5;
                rt.x = g_advdrastic.iDisplay_width * 5 / 640;
                //rt.y = y - 3;
                rt.y = y - g_advdrastic.iDisplay_height * 3 / 480;
                //rt.w = g_advdrastic.iDisplay_width - 10;
                rt.w = g_advdrastic.iDisplay_width - g_advdrastic.iDisplay_width * 10 / 640;

                rt.h = w;
                //SDL_FillRect(nds.menu.drastic.main, &rt, SDL_MapRGB(nds.menu.drastic.main->format, 
                 //   (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff));
                    
                if ((p->y == 320) || (p->y == 328)) {
                    draw_shot = 1;
                }

                if (nds.menu.show_cursor && nds.menu.drastic.cursor) {
                    //rt.x = (5 / div) + (x - nds.menu.drastic.cursor->w) / 2;
                    //rt.x = 5 + (x - nds.menu.drastic.cursor->w) / 2;
                    rt.x = g_advdrastic.iDisplay_width * 5 / 640 + (x - nds.menu.drastic.cursor->w) / 2;

                    rt.y -= ((nds.menu.drastic.cursor->h - LINE_H) / 2);
                    rt.w = 0;
                    rt.h = 0;
                    SDL_BlitSurface(nds.menu.drastic.cursor, NULL, nds.menu.drastic.main, &rt);
                }
            }

            if (p->bg)
                bgcolor = SDL_MapRGB(nds.menu.drastic.main->format, 
                    (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
            else
                bgcolor = 0;

            draw_info(nds.menu.drastic.main, buf, x, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
        }
    }

    //y = 10;
    y = g_advdrastic.iDisplay_height * 10 / 480;

    //sprintf(buf, "Rel "NDS_VER);
    sprintf(buf, "Advanced Drastic");
    
    //draw_info(nds.menu.drastic.main, buf, 10, y / div, nds.menu.c1, 0);
    //draw_info(nds.menu.drastic.main, buf, 10, y, nds.menu.c1, 0);
    draw_info(nds.menu.drastic.main, buf, g_advdrastic.iDisplay_width * 10 / 640, y, nds.menu.c1, 0);

    if (draw_shot) {
        const uint32_t len = NDS_W * NDS_H * 2;
        uint16_t *top = malloc(len);
        uint16_t *bottom = malloc(len);
	
        if (top && bottom) {
            SDL_Surface *t = NULL;
            unsigned long *param_1 = VAR_SYSTEM;
           
            uint32_t slot = g_save_slot; //*((uint32_t *)VAR_SYSTEM_SAVESTATE_NUM);
            nds_load_state_index _func = (nds_load_state_index)FUN_LOAD_STATE_INDEX;

            printf("draw_drastic_menu_main:slot=%d\n", slot);
	    
            memset(top, 0, len);
            memset(bottom, 0, len);
                
            _func((void*)VAR_SYSTEM, slot, top, bottom, 1);
   
            t = SDL_CreateRGBSurfaceFrom(top, NDS_W, NDS_H, 16, NDS_W * 2, 0, 0, 0, 0);
            if (t) {
                //rt.x = g_advdrastic.iDisplay_width - (NDS_W + (nds.enable_752x560 ? 30 : 10));
                //rt.y = nds.enable_752x560 ? h - 20 : 50;
                //rt.x = g_advdrastic.iDisplay_width - (NDS_W + 10);
                rt.x = g_advdrastic.iDisplay_width - (NDS_W + g_advdrastic.iDisplay_width * 10 / 640);

                //rt.y = 50;
                rt.y = g_advdrastic.iDisplay_height * 50 / 480;

                rt.w = NDS_W;
                rt.h = NDS_H;
                SDL_BlitSurface(t, NULL, nds.menu.drastic.main, &rt);

                SDL_FreeSurface(t);

            }

            t = SDL_CreateRGBSurfaceFrom(bottom, NDS_W, NDS_H, 16, NDS_W * 2, 0, 0, 0, 0);
            if (t) {
                //rt.x = g_advdrastic.iDisplay_width - (NDS_W + (nds.enable_752x560 ? 30 : 10));
                //rt.y = nds.enable_752x560 ? (h + NDS_H) - 20 : 50 + NDS_H;
                //rt.x = g_advdrastic.iDisplay_width - (NDS_W + 10);
                rt.x = g_advdrastic.iDisplay_width - (NDS_W + g_advdrastic.iDisplay_width  * 10 / 640);

                //rt.y = 50 + NDS_H;
                rt.y = g_advdrastic.iDisplay_height * 50 / 480 + NDS_H;
                rt.w = NDS_W;
                rt.h = NDS_H;
                SDL_BlitSurface(t, NULL, nds.menu.drastic.main, &rt);

                SDL_FreeSurface(t);
            }
        }

        if (top) {
            free(top);
        }

        if (bottom) {
            free(bottom);
        }
    }
    return 0;
}

static int mark_double_spaces(char *p)
{
    int cc = 0;
    int len = strlen(p);

    for (cc=0; cc<len - 1; cc++) {
        if ((p[cc] == ' ') && (p[cc + 1] == ' ')) {
            p[cc] = 0;
            return 0;
        }
    }
    return -1;
}

static char* find_menu_string_tail(char *p)
{
    int cc = 0;

    for (cc=strlen(p) - 1; cc>=0; cc--) {
        if (p[cc] == ' ' && p[cc-1] == ' ') {
            return &p[cc + 1];
        }
    }
    return NULL;
}



static int draw_drastic_menu_option(void)
{
    int w = 0;
    int y = 0;
    int ww = 0;
    int s0 = 0;
    int s1 = 0;
    int cc = 0;
    //int div = 1;
    int cnt = 0;
    int cursor = 0;
    SDL_Rect rt = {0};
    CUST_MENU_SUB *p = NULL;
    char buf[MAX_PATH] = {0};
    uint32_t bgcolor;

    cursor = 0;
    // [trngaje] cc=25 Configure Options
    for (cc=0; cc<drastic_menu.cnt; cc++) {
        //printf("[trngaje] cc=%d %s\n", cc, drastic_menu.item[cc].msg);
        if (drastic_menu.item[cc].bg > 0) {
            cursor = cc;
        }
    }
    //printf("[trngaje] drastic_menu.cnt:%d\n", drastic_menu.cnt);
    //printf("[trngaje] cursor:%d, %s\n", drastic_menu.item[cursor].y, drastic_menu.item[cursor].msg);
    
    if (cursor <= 6) {
        s0 = 1; // Version r2.5.2.0 을 삭제
        s1 = 14;
    }
    else if (cursor >= (drastic_menu.cnt - 7)) {
        s0 = drastic_menu.cnt  - 14;
        s1 = drastic_menu.cnt -1 ; // Configure Options  를 삭제
    }
    else {
        s0 = cursor - 6;
        s1 = cursor + 7;
    }

    for (cc=0; cc<drastic_menu.cnt; cc++) {
        //ww = LINE_H / div;
        ww = LINE_H;

        if ((cc >= s0) && (cc < s1)) {
            //y = (25 / div) + (cnt * ww);
            //y = 25 + (cnt * ww);
            y = g_advdrastic.iDisplay_height * 25 / 480 + (cnt * ww);
            memset(buf, 0, sizeof(buf));
            p = &drastic_menu.item[cc];
        
            cnt+= 1;

            if (p->bg)
                bgcolor = SDL_MapRGB(nds.menu.drastic.main->format, 
                    (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
            else
                bgcolor = 0;

            if (p->y <= 424) {
                strcpy(buf, to_lang(find_menu_string_tail(p->msg)));
                w = get_font_width(buf);
                //draw_info(nds.menu.drastic.main, buf, g_advdrastic.iDisplay_width - w - (ww / div), y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                draw_info(nds.menu.drastic.main, buf, g_advdrastic.iDisplay_width - w - ww, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);

                mark_double_spaces(p->msg);
                strcpy(buf, to_lang(p->msg));
            }
            else {
                strcpy(buf, to_lang(p->msg));
            }
            //draw_info(nds.menu.drastic.main, buf, ww / div, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
            draw_info(nds.menu.drastic.main, buf, ww, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
        }
    }
    return 0;
}

static int draw_drastic_menu_controller(void)
{
    int y = 0;
    int w = 0;
    int cc = 0;
    //int div = 1;
    int cnt = 0;
    int cursor = 0;
    SDL_Rect rt = {0};
    int s0 = 0, s1 = 0;
    CUST_MENU_SUB *p = NULL;
    char buf[MAX_PATH] = {0};

    uint32_t bgcolor, bgcolor2, bgcolor3;

    cursor = 0;
    for (cc=0; cc<drastic_menu.cnt;) {
        if ((drastic_menu.item[cc].y >= 240) && (drastic_menu.item[cc].y <= 376)) {
            if ((drastic_menu.item[cc + 1].bg > 0) || (drastic_menu.item[cc + 2].bg > 0)) {
                break;
            }
            cc+= 3;
        }
        else {
            if (drastic_menu.item[cc].bg > 0) {
                break;
            }
            cc+= 1;
        }
        cursor+= 1;
    }
    
    if (cursor <= 6) {
        s0 = 0;
        s1 = 13;
    }
    else if (cursor >= (24 - 7)) {
        s0 = 24 - 13;
        s1 = 24;
    }
    else {
        s0 = cursor - 6;
        s1 = cursor + 7;
    }

    cnt = 0;
    for (cc=0; cc<drastic_menu.cnt; cc++) {
        //w = LINE_H / div;
        w = LINE_H;
        p = &drastic_menu.item[cc];

        if ((p->y == 224) || (p->y == 232) || (p->y == 201)) {
            continue;
        }

        memset(buf, 0, sizeof(buf));
        if ((cnt >= s0) && (cnt < s1)) {
            //y = (25 / div) + ((cnt - s0) * w);
            //y = 25 + ((cnt - s0) * w);
            y = g_advdrastic.iDisplay_height * 25 / 480 + ((cnt - s0) * w);

            if ((p->y >= 240) && (p->y <= 376)) {
                if (p->bg)
                    bgcolor = SDL_MapRGB(nds.menu.drastic.main->format, 
                        (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
                else
                    bgcolor = 0;

                if (drastic_menu.item[cc + 1].bg)
                    bgcolor2 = SDL_MapRGB(nds.menu.drastic.main->format, 
                        (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
                else
                    bgcolor2 = 0;

                if (drastic_menu.item[cc + 2].bg)
                    bgcolor3 = SDL_MapRGB(nds.menu.drastic.main->format, 
                        (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
                else
                    bgcolor3 = 0;

                //draw_info(nds.menu.drastic.main, p->msg, 20 / div, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                //draw_info(nds.menu.drastic.main, p->msg, 20, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                draw_info(nds.menu.drastic.main, p->msg, g_advdrastic.iDisplay_width * 20 / 640, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                if ((p->y >= 240) && (p->y <= 376)) {
                    //draw_info(nds.menu.drastic.main, to_lang(drastic_menu.item[cc + 1].msg), 300 / div, y, drastic_menu.item[cc + 1].bg ? nds.menu.c0 : nds.menu.c1, bgcolor2);
                    //draw_info(nds.menu.drastic.main, to_lang(drastic_menu.item[cc + 1].msg), 300, y, drastic_menu.item[cc + 1].bg ? nds.menu.c0 : nds.menu.c1, bgcolor2);
                    draw_info(nds.menu.drastic.main, to_lang(drastic_menu.item[cc + 1].msg), g_advdrastic.iDisplay_width * 300 / 640, y, drastic_menu.item[cc + 1].bg ? nds.menu.c0 : nds.menu.c1, bgcolor2);
                    //draw_info(nds.menu.drastic.main, to_lang(drastic_menu.item[cc + 2].msg), 480 / div, y, drastic_menu.item[cc + 2].bg ? nds.menu.c0 : nds.menu.c1, bgcolor3);
                    //draw_info(nds.menu.drastic.main, to_lang(drastic_menu.item[cc + 2].msg), 480, y, drastic_menu.item[cc + 2].bg ? nds.menu.c0 : nds.menu.c1, bgcolor3);
                    draw_info(nds.menu.drastic.main, to_lang(drastic_menu.item[cc + 2].msg), g_advdrastic.iDisplay_width * 480 / 640, y, drastic_menu.item[cc + 2].bg ? nds.menu.c0 : nds.menu.c1, bgcolor3);
                }
            }
            else {
                if (p->bg)
                    bgcolor = SDL_MapRGB(nds.menu.drastic.main->format, 
                        (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
                else
                    bgcolor = 0;
                //draw_info(nds.menu.drastic.main, to_lang(p->msg), 20 / div, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                //draw_info(nds.menu.drastic.main, to_lang(p->msg), 20, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                draw_info(nds.menu.drastic.main, to_lang(p->msg), g_advdrastic.iDisplay_width * 20 / 640, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
            }
        }

        cnt+= 1;
        if ((p->y >= 240) && (p->y <= 376)) {
            cc+= 2;
        }
    }
    return 0;
}

static int draw_drastic_menu_controller2(void)
{
    int y = 0;
    int w = 0;
    int cc = 0;
    int cnt = 0;
    //int div = 1;
    int cursor = 0;
    SDL_Rect rt = {0};
    int s0 = 0, s1 = 0;
    CUST_MENU_SUB *p = NULL;
    uint32_t bgcolor, bgcolor2;

    cursor = 0;
    for (cc=0; cc<drastic_menu.cnt;) {
        //printf("[trngaje] y=%d, %s\n", drastic_menu.item[cc].y, drastic_menu.item[cc].msg);
        if ((drastic_menu.item[cc].y >= 240) && (drastic_menu.item[cc].y <= 536/*392*/ /*NDS_Hx2*/)) {
            if ((drastic_menu.item[cc + 1].bg > 0) || (drastic_menu.item[cc + 2].bg > 0)) {
                break;
            }
            cc+= 3;
        }
        else {
            if (drastic_menu.item[cc].bg > 0) {
                break;
            }
            cc+= 1;
        }
        cursor+= 1;
    }
    
    if (cursor <= 6) {
        s0 = 0;
        s1 = 13;
    }
    else if (cursor >= (23+NUM_OF_ADVANCE_CONTROL_INDEX - 7)) {
        s0 = 23+NUM_OF_ADVANCE_CONTROL_INDEX - 13;
        s1 = 23+NUM_OF_ADVANCE_CONTROL_INDEX;
    }
    else {
        s0 = cursor - 6;
        s1 = cursor + 7;
    }

    cnt = 0;
    for (cc=0; cc<drastic_menu.cnt; cc++) {
        //w = LINE_H / div;
        w = LINE_H;
        p = &drastic_menu.item[cc];

        if ((p->y == 224) || (p->y == 232) || (p->y == 201)) {
            continue;
        }

        if ((cnt >= s0) && (cnt < s1)) {
            //y = (25 / div) + ((cnt - s0) * w);
            //y = 25 + ((cnt - s0) * w);
            y = g_advdrastic.iDisplay_height * 25 / 480 + ((cnt - s0) * w);

            if ((p->y >= 240) && (p->y <= 536)) {
                if (drastic_menu.item[cc + 1].bg)
                    bgcolor = SDL_MapRGB(nds.menu.drastic.main->format, 
                        (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
                else
                    bgcolor = 0;

                if (drastic_menu.item[cc + 2].bg)
                    bgcolor2 = SDL_MapRGB(nds.menu.drastic.main->format, 
                        (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
                else
                    bgcolor2 = 0;                  
                //draw_info(nds.menu.drastic.main, p->msg, 20 / div, y, p->bg ? nds.menu.c0 : nds.menu.c1, 0);
                //draw_info(nds.menu.drastic.main, p->msg, 20, y, p->bg ? nds.menu.c0 : nds.menu.c1, 0);
                draw_info(nds.menu.drastic.main, p->msg, g_advdrastic.iDisplay_width * 20 / 640, y, p->bg ? nds.menu.c0 : nds.menu.c1, 0);
                //draw_info(nds.menu.drastic.main, to_lang(drastic_menu.item[cc + 1].msg), 300 / div, y, drastic_menu.item[cc + 1].bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                draw_info(nds.menu.drastic.main, to_lang(drastic_menu.item[cc + 1].msg), g_advdrastic.iDisplay_width * 300 / 640, y, drastic_menu.item[cc + 1].bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                //draw_info(nds.menu.drastic.main, to_lang(drastic_menu.item[cc + 2].msg), 480 / div, y, drastic_menu.item[cc + 2].bg ? nds.menu.c0 : nds.menu.c1, bgcolor2);
                draw_info(nds.menu.drastic.main, to_lang(drastic_menu.item[cc + 2].msg), g_advdrastic.iDisplay_width * 480 / 640, y, drastic_menu.item[cc + 2].bg ? nds.menu.c0 : nds.menu.c1, bgcolor2);
            }
            else {
                if (p->bg)
                    bgcolor = SDL_MapRGB(nds.menu.drastic.main->format, 
                        (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
                else
                    bgcolor = 0;
                //draw_info(nds.menu.drastic.main, to_lang(p->msg), 20 / div, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                //draw_info(nds.menu.drastic.main, to_lang(p->msg), 20, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                draw_info(nds.menu.drastic.main, to_lang(p->msg), g_advdrastic.iDisplay_width * 20 / 640, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
            }
        }

        cnt+= 1;
        if ((p->y >= 240) && (p->y <= 536)) {
            cc+= 2;
        }
    }
    return 0;
}

static int draw_drastic_menu_firmware(void)
{
    int t = 0;
    int w = 0;
    int y = 0;
    int ww = 30;
    int cc = 0;
    //int div = 1;
    int cnt = 0;
    SDL_Rect rt = {0};
    CUST_MENU_SUB *p = NULL;
    char buf[MAX_PATH] = {0};
    char name[MAX_PATH] = {0};
    uint32_t bgcolor;

    for (cc=0; cc<drastic_menu.cnt; cc++) {
        //ww = LINE_H / div;
        ww = LINE_H;
        p = &drastic_menu.item[cc];
        if ((p->x == 352) || (p->x == 108)) {
            continue;
        }
    
        memset(buf, 0, sizeof(buf));
        if (p->x != 92) {
            strcat(name, p->msg);
        }
        else {
            //y = (25 / div) + (cnt * ww);
            //y = 25 + (cnt * ww);
            y = g_advdrastic.iDisplay_height * 25 / 480 + (cnt * ww);

            if (p->bg)
                bgcolor = SDL_MapRGB(nds.menu.drastic.main->format, 
                    (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
            else
                bgcolor = 0;

            cnt+= 1;
            if (p->y == 280) {
                mark_double_spaces(p->msg);
                strcpy(buf, to_lang(p->msg));
            }
            else if (p->y == 296) {
                w = get_font_width(name);
                //draw_info(nds.menu.drastic.main, name, g_advdrastic.iDisplay_width - w - (ww / div), 25 / div, nds.menu.c1, bgcolor);
                //draw_info(nds.menu.drastic.main, name, g_advdrastic.iDisplay_width - w - ww, 25, nds.menu.c1, bgcolor);
                draw_info(nds.menu.drastic.main, name, g_advdrastic.iDisplay_width - w - ww, g_advdrastic.iDisplay_height *25 / 480, nds.menu.c1, bgcolor);

                w = strlen(p->msg);
                p->msg[w - 3] = 0;
                for (t=14; t<w; t++) {
                    if (p->msg[t] != ' ') {
                        strcpy(buf, &p->msg[t]);
                        break;
                    }
                }
                w = get_font_width(buf);
                //draw_info(nds.menu.drastic.main, buf, g_advdrastic.iDisplay_width - w - (ww / div), y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                draw_info(nds.menu.drastic.main, buf, g_advdrastic.iDisplay_width - w - ww, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);

                strcpy(buf, to_lang("Favorite Color"));
            }
            else if (p->y <= 312) {
                strcpy(buf, to_lang(find_menu_string_tail(p->msg)));
                w = get_font_width(buf);
                //draw_info(nds.menu.drastic.main, buf, g_advdrastic.iDisplay_width - w - (ww / div), y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                draw_info(nds.menu.drastic.main, buf, g_advdrastic.iDisplay_width - w - ww, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);

                mark_double_spaces(p->msg);
                strcpy(buf, to_lang(p->msg));
            }
            else {
                strcpy(buf, to_lang(p->msg));
            }
            //draw_info(nds.menu.drastic.main, buf, ww / div, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
            draw_info(nds.menu.drastic.main, buf, ww, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
        }
    }
    return 0;
}

static int draw_drastic_menu_cheat(void)
{
    int y = 0;
    int w = 30;
    int cc = 0;
    //int div = 1;
    int cnt = 0;
    int cursor = 0;
    SDL_Rect rt = {0};
    int s0 = 0, s1 = 0;
    CUST_MENU_SUB *p = NULL;
    char buf[MAX_PATH] = {0};
    uint32_t bgcolor;

    for (cc=0; cc<drastic_menu.cnt; cc++) {
        p = &drastic_menu.item[cc];
        if (p->x == 650) {
            for (s0=0; s0<drastic_menu.cnt; s0++) {
                if ((drastic_menu.item[s0].x == 10) && (drastic_menu.item[s0].y == p->y)) {
                    drastic_menu.item[s0].cheat = 1;
                    drastic_menu.item[s0].enable = strcmp(p->msg, "enabled") == 0 ? 1 : 0;
                    break;
                }
            }
        }
    }

    s0 = 0;
    for (cc=0; cc<drastic_menu.cnt; cc++) {
        if (drastic_menu.item[cc].x == 10) {
            memcpy(&drastic_menu.item[s0], &drastic_menu.item[cc], sizeof(drastic_menu.item[cc]));
            s0+= 1;
        }
        memset(&drastic_menu.item[cc], 0, sizeof(drastic_menu.item[cc]));
    }
    drastic_menu.cnt = s0;

    cursor = 0;
    for (cc=0; cc<drastic_menu.cnt; cc++) {
        if (drastic_menu.item[cc].bg > 0) {
            cursor = cc;
        }
    }

    if (drastic_menu.cnt == 0) {
        return 0;
    }

    if (drastic_menu.cnt < 13) {
        s0 = 0;
        s1 = drastic_menu.cnt;
    }
    else if (cursor <= 6) {
        s0 = 0;
        s1 = 13;
    }
    else if (cursor >= (drastic_menu.cnt - 7)) {
        s0 = drastic_menu.cnt - 13;
        s1 = drastic_menu.cnt;
    }
    else {
        s0 = cursor - 6;
        s1 = cursor + 7;
    }

    cnt = 0;
    for (cc=0; cc<drastic_menu.cnt; cc++) {
        //w = LINE_H / div;
        w = LINE_H;
        memset(buf, 0, sizeof(buf));
        p = &drastic_menu.item[cc];

        if (p->x != 10) {
            continue;
        }

        if ((cc >= s0) && (cc < s1)) {
            //y = (25 / div) + (cnt * w);
            //y = 25 + (cnt * w);
            y = g_advdrastic.iDisplay_height * 25 / 480 + (cnt * w);

            if (p->bg)
                bgcolor = SDL_MapRGB(nds.menu.drastic.main->format, 
                    (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
            else
                bgcolor = 0;

            cnt+= 1;
            //draw_info(nds.menu.drastic.main, p->msg, w / div, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
            draw_info(nds.menu.drastic.main, p->msg, w, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);

            if (p->cheat && nds.menu.drastic.yes && nds.menu.drastic.no) {
                //rt.x = g_advdrastic.iDisplay_width - nds.menu.drastic.yes->w - (w / div);
                rt.x = g_advdrastic.iDisplay_width - nds.menu.drastic.yes->w - w;
                rt.y = y - 1;
                rt.w = 0;
                rt.h = 0;
                SDL_BlitSurface((p->enable > 0 ) ? nds.menu.drastic.yes : nds.menu.drastic.no, NULL, nds.menu.drastic.main, &rt);
            }
        }
    }
    return 0;
}

static int draw_drastic_menu_rom(void)
{
    int y = 0;
    int w = 0;
    int cc = 0;
    //int div = 1;
    int chk = 0;
    int all = 0;
    int cnt = 0;
    int cursor = 0;
    SDL_Rect rt = {0};
    int s0 = 0, s1 = 0;
    CUST_MENU_SUB *p = NULL;
    uint32_t bgcolor;

    for (cc=0; cc<drastic_menu.cnt; cc++) {
        //printf("[trngaje] cc=%d, x=%d, y=%d, %s\n", cc, drastic_menu.item[cc].x, drastic_menu.item[cc].y, drastic_menu.item[cc].msg);
        //if (drastic_menu.item[cc].x == 10) {
            if (drastic_menu.item[cc].bg > 0) {
                //chk = 10;
                chk = drastic_menu.item[cc].x ;
                break;
            }
        //}
    }

    cursor = 0;
    for (cc=0; cc<drastic_menu.cnt; cc++) {
        if (drastic_menu.item[cc].x == chk) {
            if (drastic_menu.item[cc].bg > 0) {
                break;
            }
            cursor+= 1;
        }
    }

    all = 0;
    for (cc=0; cc<drastic_menu.cnt; cc++) {
        if (drastic_menu.item[cc].x == chk) {
            all+= 1;
        }
    }

    if (all < 12) {
        s0 = 0;
        s1 = all;
    }
    else if (cursor <= 6) {
        s0 = 0;
        s1 = 12;
    }
    else if (cursor >= (all - 6)) {
        s0 = all - 12;
        s1 = all;
    }
    else {
        s0 = cursor - 6;
        s1 = cursor + 6;
    }

    {
        uint32_t c = 0x335445;

        //w = LINE_H / div;
        w = LINE_H;
        p = &drastic_menu.item[0];
        //rt.x = 5 / div;
        //rt.x = 5;
        rt.x = g_advdrastic.iDisplay_width * 5 / 640;
        //rt.y = (25 / div) - (4 / div);
        rt.y = g_advdrastic.iDisplay_height * 21 / 480;
        //rt.w = g_advdrastic.iDisplay_width - (10 / div);
        rt.w = g_advdrastic.iDisplay_width - g_advdrastic.iDisplay_width * 10 / 640;
        rt.h = w;
        //SDL_FillRect(nds.menu.drastic.main, &rt, SDL_MapRGB(nds.menu.drastic.main->format, (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff));
        //draw_info(nds.menu.drastic.main, p->msg, 20 / div, 25 / div, 0xa0cb93, 0);
        //draw_info(nds.menu.drastic.main, p->msg, 20, 25, 0xa0cb93, 0);
        draw_info(nds.menu.drastic.main, p->msg, g_advdrastic.iDisplay_width * 20 / 640,  g_advdrastic.iDisplay_height * 25 / 480, 0xa0cb93, 0);
    }

    cnt = 0;
    for (cc=0; cc<drastic_menu.cnt; cc++) {
        //w = LINE_H / div;
        w = LINE_H;
        p = &drastic_menu.item[cc];
        if (p->x == chk) {
            //y = (25 / div) + (((cnt - s0) + 1) * w);
            //y = 25 + (((cnt - s0) + 1) * w);
            y = g_advdrastic.iDisplay_height * 25 / 480 + (((cnt - s0) + 1) * w);
            if ((cnt >= s0) && (cnt < s1)) {
                if (p->bg)
                    bgcolor = SDL_MapRGB(nds.menu.drastic.main->format, 
                        (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
                else
                    bgcolor = 0;
                //draw_info(nds.menu.drastic.main, p->msg, 20 / div, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                //draw_info(nds.menu.drastic.main, p->msg, 20, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
                draw_info(nds.menu.drastic.main, p->msg, g_advdrastic.iDisplay_width * 20 / 640, y, p->bg ? nds.menu.c0 : nds.menu.c1, bgcolor);
            }
            cnt+= 1;
        }
    }
    return 0;
}

int process_drastic_menu(void)
{
    SDL_Rect rt = {0, 0, g_advdrastic.iDisplay_width, g_advdrastic.iDisplay_height};    
    int layer = get_current_menu_layer();


   // SDL_FillRect(nds.menu.drastic.main, &rt, SDL_MapRGB(nds.menu.drastic.main->format,
    //                    0xff, 0xff, 0xff));
    SDL_FillRect(nds.menu.drastic.main, &rt, SDL_MapRGB(nds.menu.drastic.main->format,
                        0, 0, 0));
 /*                       
    if (layer == NDS_DRASTIC_MENU_MAIN) {
        SDL_SoftStretch(nds.menu.drastic.bg0, NULL, nds.menu.drastic.main, NULL);
    }
    else {
        SDL_SoftStretch(nds.menu.drastic.bg1, NULL, nds.menu.drastic.main, NULL);
    }
*/
    switch (layer) {
    case NDS_DRASTIC_MENU_MAIN:
        draw_drastic_menu_main();
        break;
    case NDS_DRASTIC_MENU_OPTION:
        draw_drastic_menu_option();
        break;
    case NDS_DRASTIC_MENU_CONTROLLER:
        draw_drastic_menu_controller();
        break;
    case NDS_DRASTIC_MENU_CONTROLLER2:
        draw_drastic_menu_controller2();
        break;
    case NDS_DRASTIC_MENU_FIRMWARE:
        draw_drastic_menu_firmware();
        break;
    case NDS_DRASTIC_MENU_CHEAT:
        draw_drastic_menu_cheat();
        break;
    case NDS_DRASTIC_MENU_ROM:
        draw_drastic_menu_rom();
        break;
    default:
        SDL_SendKeyboardKey(SDL_PRESSED, SDL_GetScancodeFromKey(SDLK_RIGHT));
        usleep(100000);
        SDL_SendKeyboardKey(SDL_RELEASED, SDL_GetScancodeFromKey(SDLK_RIGHT));
        memset(&drastic_menu, 0, sizeof(drastic_menu));
        return 0;
    }

    nds.update_menu = 1;

    memset(&drastic_menu, 0, sizeof(drastic_menu));
    return 0;
}

static int process_screen(void)
{
    static int need_loadstate = 15;
    static int show_info_cnt = 0;
    //static int cur_fb_w = 0;
    static int cur_volume = 0;
    static int cur_dis_mode = 0;
    static int cur_touchpad = 0;
    static int cur_theme_sel = 0;
    static int cur_pixel_filter = 0;
    static int pre_dis_mode = NDS_DIS_MODE_VH_S0;
    static int pre_hres_mode = NDS_DIS_MODE_HRES0;
    static char show_info_buf[MAX_PATH << 1] = {0};
    int swap_screens;
	
    int idx = 0;
    int screen_cnt = 0;
    char buf[MAX_PATH] = {0};

    unsigned long frame_count;

    int cur_sel = gfx.lcd.cur_sel ^ 1;
    unsigned char  ucLayoutType;
    unsigned short  usLayoutRotate;

    screen_cnt = 2;

   // vid.window = (SDL_Window *)(*((uint32_t *)VAR_SDL_SCREEN_WINDOW));
	frame_count =   *((uint32_t *)VAR_SDL_FRAME_COUNT);

	if (frame_count > 0) {
		frame_count-=2;
		*((uint32_t *)VAR_SDL_FRAME_COUNT) = frame_count;
	}
	
	swap_screens = (uint32_t *)(*((uint32_t *)VAR_SDL_SWAP_SCREENS));

 //   printf(PREFIX"process_screen++\n");
    if (g_advdrastic.uiEnableautosavestate > 0) {
        if (need_loadstate > 0) { // 꼭 이래야 하나 확인 필요함 (15 frame 후에 실행)
            need_loadstate-= 1;
            if (need_loadstate == 0) {
                dtr_loadstate(g_advdrastic.uiSlotOfAutosavestate);
            }
        }
    }

    if (nds.menu.enable) {
        need_reload_bg = RELOAD_BG_COUNT;
        return 0;
    }

/*
    if (nds.menu.drastic.enable) {
        nds.menu.drastic.enable = 0;
        need_reload_bg = RELOAD_BG_COUNT;
#ifdef QX1000
        update_wayland_res(NDS_W * 2, NDS_H);
#endif
    }
*/

    if ((nds.shot.take) ||
        (cur_dis_mode != g_advdrastic.ucLayoutIndex[nds.hres_mode] /*nds.dis_mode*/) ||
        (cur_theme_sel != nds.theme.sel) ||
        (cur_pixel_filter != pixel_filter) ||
        (cur_volume != nds.volume))
    {
        if (cur_volume != nds.volume) {
            show_info_cnt = 50;
            sprintf(show_info_buf, " %s %d ", to_lang("Volume"), nds.volume);
        }
        else if (cur_theme_sel != nds.theme.sel) {
            //printf("[trngaje] cur_theme_sel=%d, nds.theme.sel=%d, nds.theme.max=%d\n", cur_theme_sel,  nds.theme.sel, nds.theme.max);
            show_info_cnt = 50;
            if ((nds.theme.max > 0) && (nds.theme.sel < nds.theme.max)) {
                if (get_dir_path(nds.theme.path, nds.theme.sel, buf) == 0) {
                    int off = strlen(nds.theme.path);
                    sprintf(show_info_buf, " %s %s ", to_lang("Wallpaper"), &buf[off + 1]);
                }
                else {
                    sprintf(show_info_buf, " %s %d ", to_lang("Wallpaper"), nds.theme.sel);
                }
            }
            else {
                sprintf(show_info_buf, " %s %s ", to_lang("Wallpaper"), to_lang("None"));
            }
        }
        else if (cur_pixel_filter != pixel_filter) {
            show_info_cnt = 50;
            sprintf(show_info_buf, " %s ", to_lang(pixel_filter ? "Pixel" : "Blur"));
        }
        else if (nds.shot.take) {
            show_info_cnt = 50;
            nds.shot.take = 0;
            sprintf(show_info_buf, " %s ", to_lang("Take Screenshot"));
        }

        cur_theme_sel = nds.theme.sel;
        cur_volume = nds.volume;
        cur_dis_mode = g_advdrastic.ucLayoutIndex[nds.hres_mode];
        cur_pixel_filter = pixel_filter;
        need_reload_bg = RELOAD_BG_COUNT;
    }

    if (show_info_cnt == 0) {
        need_reload_bg = RELOAD_BG_COUNT;
        show_info_cnt = -1;
    }
        
    if (nds.defer_update_bg > 0) {
        nds.defer_update_bg-= 1;
    }
    else if (nds.defer_update_bg == 0) {
        nds.defer_update_bg = -1;
        need_reload_bg = RELOAD_BG_COUNT;
    }

    if (nds.state) {
        if (nds.state & NDS_STATE_QSAVE) {
            show_info_cnt = 50;
            sprintf(show_info_buf, " %s ", to_lang("Quick Save"));
            nds.state&= ~NDS_STATE_QSAVE;
        }
        else if (nds.state & NDS_STATE_QLOAD) {
            show_info_cnt = 50;
            sprintf(show_info_buf, " %s ", to_lang("Quick Load"));
            nds.state&= ~NDS_STATE_QLOAD;
        }
        else if (nds.state & NDS_STATE_FF) {
            show_info_cnt = 50;
            sprintf(show_info_buf, " %s ", to_lang("Fast Forward"));
            nds.state&= ~NDS_STATE_FF;
        }
    }

    nds.screen.bpp = *((uint32_t *)VAR_SDL_SCREEN_BPP);
//printf(PREFIX"process_screen+bpp=%d\n", nds.screen.bpp);
    nds.screen.init = *((uint32_t *)VAR_SDL_SCREEN_NEED_INIT);

//printf(PREFIX"process_screen+init=%d\n", nds.screen.init);

    if (need_reload_bg) {
        reload_bg();
        need_reload_bg -= 1;
    }

    for (idx = 0; idx < screen_cnt; idx++) {
        int screen0 = 0;
        int screen1 = 0;
        int show_pen = 1;
        int need_update = 1;
        int rotate = E_MI_GFX_ROTATE_180; // 이거 때문에 뒤집히는 것 같음
        SDL_Rect srt = {0, 0, NDS_W, NDS_H};
        SDL_Rect drt = {0, 0, 160, 120};

        nds.screen.hres_mode[idx] = idx ?
            *((uint8_t *)VAR_SDL_SCREEN1_HRES_MODE):
            *((uint8_t *)VAR_SDL_SCREEN0_HRES_MODE);

        nds.screen.pixels[idx] = (idx == 0) ?
            (uint64_t *)(*((uint64_t *)VAR_SDL_SCREEN0_PIXELS)):
            (uint64_t *)(*((uint64_t *)VAR_SDL_SCREEN1_PIXELS));

        screen0 = (idx == 0);
        screen1 = (idx != 0);
        show_pen = nds.pen.pos ? screen1 : screen0;
            
        if (nds.screen.hres_mode[idx]) {
            srt.w = NDS_Wx2;
            srt.h = NDS_Hx2;
            nds.screen.pitch[idx] = nds.screen.bpp * srt.w;
            if (nds.hres_mode == 0) {
                nds.pen.pos = 0;
                nds.hres_mode = 1;
                pre_dis_mode = g_advdrastic.ucLayoutIndex[nds.hres_mode];
                nds.dis_mode = pre_hres_mode;
            }
        }
        else {
            srt.w = NDS_W;
            srt.h = NDS_H;
            nds.screen.pitch[idx] = nds.screen.bpp * srt.w;

            drt.y = idx * 120;

            if (nds.hres_mode == 1) {
                nds.hres_mode = 0;
                pre_hres_mode = g_advdrastic.ucLayoutIndex[nds.hres_mode];
                nds.dis_mode = pre_dis_mode;
            }
        }

        usLayoutRotate = getlayout_rotate(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);
        ucLayoutType = getlayout_type(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);

        switch (ucLayoutType) {
        case LAYOUT_TYPE_TRANSPARENT:
        case LAYOUT_TYPE_SINGLE:
            if (screen1) {
                getlayout_size(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode], idx, &drt);
            }
            else {
                need_update = 0;
            }
            break;

        case LAYOUT_TYPE_VERTICAL:
            getlayout_size(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode], idx, &drt);

/*
            rotate = (usLayoutRotate == 90) ? E_MI_GFX_ROTATE_90 : E_MI_GFX_ROTATE_270;  // 사용 안하는 코드
*/
            break;

        default:
            getlayout_size(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode], idx, &drt);
            break;
        }

         nds.pen.pos = swap_screens ^ 1; // trngaje;
/*
        if (rotate == E_MI_GFX_ROTATE_180) { // 이부분 코드 확인 필요함
            drt.y = (g_advdrastic.iDisplay_height - drt.y) - drt.h;
            drt.x = (g_advdrastic.iDisplay_width - drt.x) - drt.w;
        }
*/
        if  (show_pen && (frame_count > 0/* || (evt.mode == MMIYOO_MOUSE_MODE || evt.pen_display_cnt > 0)*/)) {
/*
            if (evt.pen_display_cnt > 0) {
                evt.pen_display_cnt--;
            }
*/
            draw_pen(nds.screen.pixels[idx], srt.w, nds.screen.pitch[idx]);
        }

        if ((idx == 0) && (nds.alpha.border > 0) && (ucLayoutType == LAYOUT_TYPE_TRANSPARENT)) {
            int c0 = 0;
            uint32_t *p0 = NULL;
            uint32_t *p1 = NULL;
            uint32_t col[] = { 0, 0xffffff, 0xff0000, 0x00ff00, 0x0000ff, 0x000000, 0xffff00, 0x00ffff };

            p0 = (uint32_t *)nds.screen.pixels[idx];
            p1 = (uint32_t *)nds.screen.pixels[idx] + ((srt.h - 1) * srt.w);
            for (c0 = 0; c0 < srt.w; c0++) {
                *p0++ = col[nds.alpha.border];
                *p1++ = col[nds.alpha.border];
            }

            p0 = (uint32_t *)nds.screen.pixels[idx];
            p1 = (uint32_t *)nds.screen.pixels[idx] + (srt.w - 1);
            for (c0 = 0; c0 < srt.h; c0++) {
                *p0 = col[nds.alpha.border];
                *p1 = col[nds.alpha.border];
                p0 += srt.w;
                p1 += srt.w;
            }
        }
        glBindTexture(GL_TEXTURE_2D, vid.texID[idx]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (pixel_filter) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, srt.w, srt.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nds.screen.pixels[idx]);

        if (need_update) {
            GFX_Copy(idx, nds.screen.pixels[idx], srt, drt, nds.screen.pitch[idx], 0, rotate);

            switch (ucLayoutType) {
            case LAYOUT_TYPE_TRANSPARENT:
                getlayout_size(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode], 0, &drt);

                switch (nds.alpha.pos) {
                case 0:
                    drt.x = g_advdrastic.iDisplay_width - drt.w;
                    drt.y = 0;
                    break;
                case 1:
                    drt.x = 0;
                    drt.y = 0;
                    break;
                case 2:
                    drt.x = 0;
                    drt.y = g_advdrastic.iDisplay_height - drt.h;
                    break;
                case 3:
                    drt.x = g_advdrastic.iDisplay_width - drt.w;
                    drt.y = g_advdrastic.iDisplay_height - drt.h;
                    break;
                }

                GFX_Copy(TEX_SCR0, nds.screen.pixels[0], srt, drt, nds.screen.pitch[0], 1, rotate);

                break;
            }
        }

        if (nds.screen.hres_mode[idx]) {
            break;
        }
    }

    if (show_info_cnt > 0) {
        draw_info(NULL, show_info_buf, g_advdrastic.iDisplay_width - get_font_width(show_info_buf), 0, 0xe0e000, 0x000000);
        show_info_cnt-= 1;
    }
    else if (show_fps && fps_info) {
        SDL_Rect rt = {0};

        rt.x = g_advdrastic.iDisplay_width - fps_info->w;
        rt.y = 0;
        rt.w = fps_info->w;
        rt.h = fps_info->h;
        GFX_Copy(-1, fps_info->pixels, fps_info->clip_rect, rt, fps_info->pitch, 0, E_MI_GFX_ROTATE_180);
    }

    if (nds.screen.init) {
        nds_set_screen_menu_off _func = (nds_set_screen_menu_off)FUN_SET_SCREEN_MENU_OFF;
        _func();
    }
//printf(PREFIX"process_screen+step5\n");
//    GFX_Flip(); // 이거를 thread로 이동해 본다.
//printf(PREFIX"process_screen--\n");
    return 0;
}

void sdl_blit_screen_menu(uint16_t *src, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
}

void sdl_update_screen_menu(void)
{
/*
    int SDL_UpdateTexture(SDL_Texture * texture,
                  const SDL_Rect * rect,
                  const void *pixels, int pitch);
                  
    int SDL_RenderCopy(SDL_Renderer * renderer,
               SDL_Texture * texture,
               const SDL_Rect * srcrect,
               const SDL_Rect * dstrect);
               
    void SDL_RenderPresent(SDL_Renderer * renderer);               
*/

//    SDL_UpdateTexture(base_addr_rx + 0x03f2a588,0,base_addr_rx + 0x03f2a5a0, 0x640);
//    SDL_RenderCopy(base_addr_rx + 0x03f2a580, base_addr_rx + 0x03f2a588, 0, 0);
//    SDL_RenderPresent(base_addr_rx + 0x03f2a580);

    // acquire a lock
    pthread_mutex_lock(&lock);
    
    nds.update_screen = 1;
    pthread_cond_signal(&request_update_screen_cond);

    process_drastic_menu();
        
    pthread_cond_wait(&response_update_screen_cond, &lock);
    
    // release lock 
    pthread_mutex_unlock(&lock);  
}



void sdl_setscreen_orientation(long lparm_1) // not used
{
    g_advdrastic.ucLayoutIndex[nds.hres_mode]= ( g_advdrastic.ucLayoutIndex[nds.hres_mode] + 1) % getmax_layout(nds.hres_mode);
}

void sdl_get_gui_input(long param_1,unsigned int *param_2)
{
    printf("[trngaje] sdl_get_gui_input:param_1=%p\n", param_1 - base_addr_rx);
}

void display_custom_setting(void)
{
    unsigned int input_value[2];
    nds_get_gui_input get_gui_input;
    int guikey=-1;
    
    nds_clear_gui_actions clear_gui_actions;
    clear_gui_actions = (nds_clear_gui_actions)FUN_CLEAR_GUI_ACTIONS;
    
    // acquire a lock
    pthread_mutex_lock(&lock);
    
    get_gui_input = (nds_get_gui_input)FUN_GET_GUI_INPUT;
    nds.menu.enable = 1;
    do {
        handle_menu(guikey);
        GFX_Copy(-1, cvt->pixels, cvt->clip_rect, cvt->clip_rect, cvt->pitch, 0, E_MI_GFX_ROTATE_180);
        GFX_Flip();
        
        pthread_cond_signal(&request_update_screen_cond);
        pthread_cond_wait(&response_update_screen_cond, &lock);
        
        do {
            get_gui_input(base_addr_rx + 0x3fb528, input_value);
        } while (input_value[0] == 0xb);

            
        printf("[trngaje] get_gui_input=0x%x, 0x%x\n", input_value[0], input_value[1]);
               
        guikey = -1;
        switch(input_value[0]) {
        case DRASTIC_GUI_INPUT_UP: 
        case DRASTIC_GUI_INPUT_DOWN: 
        case DRASTIC_GUI_INPUT_LEFT: 
        case DRASTIC_GUI_INPUT_RIGHT: 
        case DRASTIC_GUI_INPUT_A: 
        case DRASTIC_GUI_INPUT_B: 
            guikey = input_value[0];
        }
    } while(nds.menu.enable == 1 && guikey != DRASTIC_GUI_INPUT_B);
    nds.menu.enable = 0;
    clear_gui_actions();
    
    // release lock 
    pthread_mutex_unlock(&lock); 
}

void sdl_screen_set_cursor_position(unsigned int param_1, unsigned int param_2, unsigned int param_3)
{
	//printf("screen_set_cursor_position : 0x%x, 0x%x, 0x%x\n", param_1,  param_2, param_3);
/*
	DAT_03f2a5e4._0_4_ = param_1;
	DAT_03f2a5e4._4_4_ = param_2;
	DAT_03f2a5ec._4_4_ = param_3;
	DAT_03f2a5f4 = 300;
*/	
	
	*((uint64_t *)(base_addr_rx + 0x03f2a5e4)) = param_1;
	*((uint64_t *)(base_addr_rx + 0x03f2a5e4 + 4)) = param_2;
	*((uint64_t *)(base_addr_rx + 0x03f2a5ec + 4)) = param_3;
	*((uint64_t *)(base_addr_rx + 0x03f2a5f4)) = 300;
	return;
}

void sdl_platform_get_input(long param_1)
{
	long lVar1;
	int iVar2;
	unsigned int uVar3;
	uint uVar4;
	ulong uVar5;
	unsigned int uVar6;
	uint uVar7;
	uint uVar8;
	uint uVar9;
	uint uVar10;
	int iVar11;
	uint uVar12;
	//unsigned long  uVar13;
	//uint local_40 [2];
	//uint local_38;
	//byte local_34;
	//byte local_33;
	//short local_30;
	//uint local_2c;
	//float local_28;
	//float local_24;
	//int iStack_20;
	// 위 local 변수 event로 변경
	SDL_Event event;
	unsigned long ulJoyAxis_Var9, ulJoyAxis_Var10;
    unsigned short  usLayoutRotate;

    nds_screen_set_cursor_position screen_set_cursor_position;
    nds_convert_touch_coordinates convert_touch_coordinates;
    
    screen_set_cursor_position = (nds_screen_set_cursor_position)FUN_SCREEN_SET_CURSOR_POSITION;
    convert_touch_coordinates = (nds_convert_touch_coordinates)FUN_CONVERT_TOUCH_COORDINATES;
    
	//printf("[trngaje] sdl_platform_get_input++\n");
    // SDL_input 
    // 03f2a608
    // VAR_SDL_INPUT
    // (uint32_t) (*((uint64_t *)VAR_SDL_INPUT));
   
    lVar1 = *(long *)(param_1 + 0x80008) + 0x85ad0;
    //SDL_input._2072_8_ = 0; // 03f2ae20
    *((uint64_t *)(VAR_SDL_INPUT + 2072)) = 0;
    //SDL_input._2088_4_ = 0xffffffff; // 03f2ae30
    *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 0xffffffff;
    uVar12 = *(uint *)(param_1 + 0x80010);
	
	//printf("[trngaje] sdl_platform_get_input:step1\n");
LAB_0009ab68:
    //iVar2 = SDL_PollEvent(local_40); // int SDL_PollEvent(SDL_Event * event);
	iVar2 = SDL_PollEvent(&event); 
	//printf("[trngaje] sdl_platform_get_input:step2=0x%x\n", iVar2);
    if (iVar2 != 0) {
        while (event.type != SDL_JOYAXISMOTION) {  // 0x600
			//printf("[trngaje] sdl_platform_get_input:step4: 0x%x\n", event.type);
            if (SDL_JOYAXISMOTION < event.type) {
                if (event.type == SDL_JOYBUTTONUP) { // 0x604
                    //uVar7 = (uint)*(unsigned long *)
                    //             (lVar1 + (long)(int)((local_38 & 3) << 8 | local_34 | 0x400) * 8);
					uVar7 = (uint)*(unsigned long *)
                                 (lVar1 + (long)(int)((event.window.windowID & 3) << 8 | event.window.event | 0x400) * 8);
					 //         uVar6 = *(uint *)((psVar8->config).controls_code_to_config_map +
                     //      (event.window.event | 0x400 | (event.window.windowID & 3) << 8));
                    g_advdrastic.ulInput &=  (*(unsigned long *)(lVar1 + (long)(int)((event.window.windowID & 3) << 8 | event.window.event | 0x400) * 8)) ^ 0xffffffffffffffff;
                    
                    if ((uVar7 >> 0x11 & 1) != 0) {
                        //SDL_input._2088_4_ = 0; // 03f2ae30
                        *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 0;
                    }
joined_r0x0009af98:
                    uVar12 = uVar12 & (uVar7 ^ 0xffffffff);
                    if ((uVar7 >> 0xe & 1) != 0) {
                        //SDL_input._2084_4_ = 0; // 03f2ae30
                        *((uint32_t *)(VAR_SDL_INPUT + 2084)) = 0;
                    }
                    if ((uVar7 >> 0x10 & 1) != 0) {
                        //SDL_input._2080_4_ = 0; // 03f2ae28
                        *((uint32_t *)(VAR_SDL_INPUT + 2080)) = 0;
                    }
                    if ((uVar7 >> 0xd & 1) != 0) {
                        //SDL_input._2084_4_ = 0; // 03f2ae30
                        *((uint32_t *)(VAR_SDL_INPUT + 2084)) = 0;
                    }
                    if ((uVar7 >> 0xf & 1) != 0) {
                        //SDL_input._2080_4_ = 0; // 03f2ae28
                        *((uint32_t *)(VAR_SDL_INPUT + 2080)) = 0;
                    }
                }
                else if (event.type < SDL_JOYDEVICEADDED) { // 0x605
                    if (event.type == SDL_JOYHATMOTION) { // 0x602
                        unsigned long ulLeft, ulRight, ulUp,ulDown;
                        
                        //uVar7 = (local_38 & 3) << 8;
						uVar7 = (event.window.windowID  & 3) << 8;
                        
                        ulDown = *(unsigned long *)(lVar1 + (long)(int)(uVar7 | 0x444) * 8); // down
                        ulUp = *(unsigned long *)(lVar1 + (long)(int)(uVar7 | 0x441) * 8); // up
                        ulRight = *(unsigned long *)(lVar1 + (long)(int)(uVar7 | 0x442) * 8); // right
                        ulLeft = *(unsigned long *)(lVar1 + (long)(int)(uVar7 | 0x448) * 8); // left
                        
                        uVar9 = (uint)*(unsigned long *)(lVar1 + (long)(int)(uVar7 | 0x444) * 8); // down
                        uVar10 = (uint)*(unsigned long *)(lVar1 + (long)(int)(uVar7 | 0x441) * 8); // up
                        uVar4 = (uint)*(unsigned long *)(lVar1 + (long)(int)(uVar7 | 0x442) * 8); // right
                        uVar8 = (uint)*(unsigned long *)(lVar1 + (long)(int)(uVar7 | 0x448) * 8); // left
                        uVar12 = uVar12 & ((uVar4 | uVar8 | uVar9 | uVar10) ^ 0xffffffff);
                        
                        g_advdrastic.ulInput &= (ulUp | ulDown | ulLeft | ulRight) ^ 0xffffffffffffffff;
                        
                        uVar7 = uVar12 | uVar10;
                        //printf("[trngaje] SDL_JOYHATMOTION: 0x%x\n", event.window.padding1);
                        if ((event.window.padding1 & 1) == 0) { // up
                            uVar7 = uVar12;
                        }
                        else {
                            g_advdrastic.ulInput |= ulUp;
                        }
                        
                        uVar12 = uVar7 | uVar9;
                        if ((event.window.padding1 & 4) == 0) { // down
                            uVar12 = uVar7;
                        }
                        else {
                            g_advdrastic.ulInput |= ulDown;
                        }
                        
                        uVar7 = uVar12 | uVar8;
                        if ((event.window.padding1 & 8) == 0) { // left
                            uVar7 = uVar12;
                        }
                        else {
                            g_advdrastic.ulInput |= ulLeft;
                        }
                        
                        uVar12 = uVar7 | uVar4;
                        if ((event.window.padding1 & 2) == 0) { // right
                            uVar12 = uVar7;
                        }
                        else {
                            g_advdrastic.ulInput |= ulRight;
                        }
                        
                        //printf("[trngaje] SDL_JOYHATMOTION:uVar12 = %p\n", g_advdrastic.ulInput);
                    }
                    else if (event.type == SDL_JOYBUTTONDOWN) { // 0x603
                        uVar7 = (uint)*(unsigned long *)
                                       (lVar1 + (long)(int)((event.window.windowID & 3) << 8 | event.window.event | 0x400) * 8);
                                       
                        
                        uVar12 = uVar12 | uVar7;
                        g_advdrastic.ulInput |=  *(unsigned long *)(lVar1 + (long)(int)((event.window.windowID & 3) << 8 | event.window.event | 0x400) * 8);
                        
//[trngaje] uVar7 = 0x0, , id=0x0, event=0xb, offset=0x2058, button=0xb, state=0x1
//[trngaje] uVar7 = 0x0, , id=0x0, event=0xe, offset=0x2070, button=0xe, state=0x1


                        //printf("[trngaje] uVar7 = %p id=0x%x, event=0x%x, offset=0x%x, button=0x%x, state=0x%x\n", 
                         //   *(unsigned long *)(lVar1 + (long)(int)((event.window.windowID & 3) << 8 | event.window.event | 0x400) * 8), 
                         //   event.window.windowID, event.window.event, (long)(int)((event.window.windowID & 3) << 8 | event.window.event | 0x400),
                          //  event.jbutton.button, event.jbutton.state);
                        if ((uVar7 >> 0x11 & 1) != 0) {
                            //SDL_input._2088_4_ = 1; // 03f2ae30
                            *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 1;
                        }
                        if ((uVar7 >> 0xe & 1) != 0) {
                            //SDL_input._2084_4_ = 20000; // 03f2ae30
                            *((uint32_t *)(VAR_SDL_INPUT + 2084)) = 20000;
                        }
                        if ((uVar7 >> 0x10 & 1) != 0) {
                            //SDL_input._2080_4_ = 20000; // 03f2ae28
                            *((uint32_t *)(VAR_SDL_INPUT + 2080)) = 20000;
                        }
                        if ((uVar7 >> 0xd & 1) != 0) {
                            //SDL_input._2084_4_ = 0xffffb1e0; // 03f2ae30
                            *((uint32_t *)(VAR_SDL_INPUT + 2084)) = 0xffffb1e0;
                        }
joined_r0x0009ad08:
                        if ((uVar7 >> 0xf & 1) != 0) {
                            //SDL_input._2080_4_ = 0xffffb1e0; // 03f2ae28
                            *((uint32_t *)(VAR_SDL_INPUT + 2080)) = 0xffffb1e0;
                        }
                    }
                }
                else if (event.type == SDL_FINGERUP) { // 0x701
                    *(unsigned char *)(param_1 + 0x8001c) = 0;
                }
                else if ((event.type == SDL_FINGERMOTION) || (event.type == SDL_FINGERDOWN)) { // 0x702,  0x700
                    //convert_touch_coordinates((int)local_28,(int)local_24,param_1 + 0x80014,param_1 + 0x80018,
                    //         *(unsigned int *)(*(long *)(param_1 + 0x80008) + 0x859d4));
                    convert_touch_coordinates((int)event.window.data2,(int)event.wheel.direction,param_1 + 0x80014,param_1 + 0x80018,
                             *(unsigned int *)(*(long *)(param_1 + 0x80008) + 0x859d4));
					//convert_touch_coordinates
					//			(event.window.data2,event.wheel.direction,&pending_touch_x,&pending_touch_y,
					//			 (psVar11->config).mirror_touch);
				 
                    *(unsigned char *)(param_1 + 0x8001c) = 1;
                }
                goto LAB_0009ab68;
            }
			
            if (event.type == SDL_KEYUP) { // 0x301
			    printf("[trngaje] sdl_platform_get_input:SDL_KEYUP\n");
                uVar7 = (uint)*(unsigned long *)
                               (lVar1 + (long)(int)(((int)event.window.data2 >> 0x1e & 3U) << 8 | event.window.data2 & 0xff) * 8 // local_2c = event.window.data2
                               );
                goto joined_r0x0009af98;
            }
			
            if (event.type < SDL_TEXTEDITING) { // 0x302
                if (event.type != SDL_QUIT) { // 0x100
                    if (event.type != SDL_KEYDOWN) goto LAB_0009ab68; // 0x300
					printf("[trngaje] sdl_platform_get_input:SDL_KEYDOWN\n");
                    if (event.window.data2 != 0x1b) {
                    uVar5 = SDL_GetModState();
                    uVar7 = (uint)*(unsigned long *)
                                   (lVar1 + (long)(int)(((int)event.window.data2 >> 0x1e & 3U) << 8 | event.window.data2 & 0xff)
                                            * 8);
                    uVar12 = uVar12 | uVar7;
                    uVar4 = uVar12;
                    if (((uVar12 >> 0x1c & 1) != 0) &&
                        (uVar4 = uVar12 & 0xefffffff | 0x20000000, (uVar5 & 3) == 0)) {
                        uVar4 = uVar12;
                    }
                    if ((uVar4 >> 0x14 & 1) != 0) {
                        uVar12 = uVar4 | 0x10000000;
                        if ((uVar5 & 1) == 0) {
                            uVar12 = uVar4;
                        }
                        uVar4 = uVar12 | 0x20000000;
                        if ((uVar5 & 2) == 0) {
                            uVar4 = uVar12;
                        }
                    }
                    uVar12 = uVar4;
                    if (((uVar4 >> 0x12 & 1) != 0) &&
                        (uVar12 = uVar4 & 0xfffbffff | 0x2000000, (uVar5 & 3) == 0)) {
                        uVar12 = uVar4;
                    }
                    if ((uVar7 >> 0xe & 1) != 0) {
                        //SDL_input._2084_4_ = 20000; // 03f2ae30
                        *((uint32_t *)(VAR_SDL_INPUT + 2084)) = 20000;
                    }
                    if ((uVar7 >> 0x10 & 1) != 0) {
                        //SDL_input._2080_4_ = 20000; // 03f2ae28
                        *((uint32_t *)(VAR_SDL_INPUT + 2080)) = 20000;
                    }
                    if ((uVar7 >> 0xd & 1) != 0) {
                        //SDL_input._2084_4_ = 0xffffb1e0; // 03f2ae30
                        *((uint32_t *)(VAR_SDL_INPUT + 2084)) = 0xffffb1e0;
                    }
                    goto joined_r0x0009ad08;
                }
            }
            uVar12 = uVar12 | 0x4000000;
            goto LAB_0009ab68;
        }
		
        if (event.type == SDL_MOUSEBUTTONDOWN) { // 0x401
		    //printf("[trngaje] sdl_platform_get_input:SDL_MOUSEBUTTONDOWN\n");
            //SDL_input._2088_4_ = 1;
            *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 1;
            goto LAB_0009ab68;
        }
		
        if (event.type == SDL_MOUSEBUTTONUP) { // 0x402
			//printf("[trngaje] sdl_platform_get_input:SDL_MOUSEBUTTONUP\n");
            //SDL_input._2088_4_ = 0;
            *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 0;
            goto LAB_0009ab68;
        }
		
        if (event.type != SDL_MOUSEMOTION) goto LAB_0009ab68; // 0x400
		// 여긴 마우스 이벤트
		//printf("[trngaje] sdl_platform_get_input:SDL_MOUSEMOTION\n");
		//printf("[trngaje] x=0x%x, y=0x%x,xrel=0x%x,yrel=0x%x,dir=0x%x\n", event.motion.x, event.motion.y,event.motion.xrel, event.motion.yrel, event.wheel.direction); 
        // 여기 변수 값 확인 할 것 local_24, iStack_20 : 존재하지 않음
        //uVar13._0_4_ = (int)local_24 + (int)SDL_input._2072_8_;
        //uVar13._4_4_ = iStack_20 + SUB84(SDL_input._2072_8_,4);
		
		 *((uint32_t *)(VAR_SDL_INPUT + 2072)) = event.motion.xrel;
		 *((uint32_t *)(VAR_SDL_INPUT + 2076)) = event.motion.yrel;
		  
        //iVar2 = SDL_PollEvent(local_40);
		iVar2 = SDL_PollEvent(&event);
        if (iVar2 == 0) goto LAB_0009abc8; // 여기가 마지막
    }
	
	//printf("[trngaje] sdl_platform_get_input:step10\n");
    iVar11 = (int)event.jaxis.value; // local_30=event.jaxis.value;
    uVar9 = (event.window.windowID & 3) << 8 | (uint)event.window.event;
    uVar7 = (uint)event.window.event << 1;
    // -32768 ~ 32767
    //iVar2 = ((int)event.jaxis.value / 20000 + ((int)event.jaxis.value >> 0x1f)) - (iVar11 >> 0x1f); // -1, 0, 1
    iVar2 = (int)event.jaxis.value / 8000; // -1, 0, 1

    //uVar8 = *(uint *)(SDL_input + (ulong)local_38 * 4) & (3 << (ulong)(uVar7 & 0x1f) ^ 0xffffffffU);
       //printf("[trngaje] step1: iVar11=%d, uVar9=%p, uVar7 = %p, iVar2=%d\n", iVar11, uVar9, uVar7, iVar2); 
    uVar8 = *(uint32_t *)(VAR_SDL_INPUT + (ulong)event.window.windowID * 4) & (3 << (ulong)(uVar7 & 0x1f) ^ 0xffffffffU);
    uVar4 = 1 << (ulong)(uVar7 & 0x1f);
    uVar7 = 2 << (ulong)(uVar7 & 0x1f);
    
    //printf("[trngaje] iVar2=%d/%d\n", iVar2, (int)event.jaxis.value);
    //printf("[trngaje] step2:uVar8=%p,uVar4=%p, uVar7= %p\n", uVar8, uVar4, uVar7);
    
    if (iVar11 < 0x2711) {
        if (iVar11 < -10000) {
            //printf("[trngaje] step2-1\n"); // left or up
            uVar8 = uVar8 | uVar7;
            uVar4 = uVar4 & uVar8;
        }
        else {
            //printf("[trngaje] step2-2\n"); // center
            uVar4 = uVar4 & uVar8;
            uVar7 = uVar7 & uVar8;
        }
    }
    else {
        //printf("[trngaje] step2-3\n"); // right or down
        uVar8 = uVar8 | uVar4;
        uVar7 = uVar7 & uVar8;
    }
	
	//printf("[trngaje] step3:uVar8=%p,uVar4=%p, uVar7= %p\n", uVar8, uVar4, uVar7); // uVar8 이 direction 정보가 계산
    
    ulJoyAxis_Var10 = *(unsigned long *)(lVar1 + (ulong)(uVar9 | 0x4c0) * 8);
    ulJoyAxis_Var9 = *(unsigned long *)(lVar1 + (ulong)(uVar9 | 0x480) * 8);
    
    //printf("[trngaje] step4:ulJoyAxis_Var10=%p,ulJoyAxis_Var9=%p\n", ulJoyAxis_Var10, ulJoyAxis_Var9);
    uVar10 = (uint)ulJoyAxis_Var10;
    uVar9 = (uint)ulJoyAxis_Var9;
    
    
    
    uVar12 = uVar12 & ((uVar10 | uVar9) ^ 0xffffffff);
    
    g_advdrastic.ulInput &= (ulJoyAxis_Var10 | ulJoyAxis_Var9) ^ 0xffffffffffffffff;
    
	
	// [trngaje] sdl_platform_get_input:SDL_JOYAXISMOTION=0x2000, 0x4000, 0x0   ; left stick up/down
	// [trngaje] sdl_platform_get_input:SDL_JOYAXISMOTION=0x8000, 0x10000, 0x0 ; left stick left/right
	// [trngaje] sdl_platform_get_input:SDL_JOYAXISMOTION=0x0, 0x0, 0x0 ; right stick

	//printf("[trngaje] sdl_platform_get_input:SDL_JOYAXISMOTION=0x%x, 0x%x, 0x%x\n", uVar10, uVar9, uVar12);

    //[trngaje] sdl_platform_get_input:SDL_JOYAXISMOTION=ulJoyAxis_Var10=0x40000000000,    ulJoyAxis_Var9=0x80000000000
    //[trngaje] sdl_platform_get_input:SDL_JOYAXISMOTION=ulJoyAxis_Var10=0x200000000000, ulJoyAxis_Var9=0x100000000000

    //printf("[trngaje] sdl_platform_get_input:SDL_JOYAXISMOTION=ulJoyAxis_Var10=%p, ulJoyAxis_Var9=%p\n", ulJoyAxis_Var10, ulJoyAxis_Var9);

    //*(uint *)(SDL_input + (ulong)local_38 * 4) = uVar8;
    *(uint32_t *)(VAR_SDL_INPUT + (ulong)event.window.windowID * 4) = uVar8;
    if (((uVar10 | uVar9) >> 0x11 & 1) != 0) {
        //SDL_input._2088_4_ = 0; // 03f2ae30
        *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 0;
        //printf("[trngaje] step5-1\n");
    }
    if (uVar4 != 0) {
        //printf("[trngaje] step5-2\n"); // 여기만 진입함
        if ((uVar9 >> 0x11 & 1) != 0) {
            //SDL_input._2088_4_ = 1; // 03f2ae30
            *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 1;
            //printf("[trngaje] step5-3\n");
        }
        uVar12 = uVar12 | uVar9;
        g_advdrastic.ulInput |= ulJoyAxis_Var9;
    }
    if (uVar7 != 0) {
        if ((uVar10 >> 0x11 & 1) != 0) { // 여기만 진입함
            //SDL_input._2088_4_ = 1; // 03f2ae30
            *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 1;
        }
        uVar12 = uVar12 | uVar10;
        g_advdrastic.ulInput |= ulJoyAxis_Var10;
        }
        if ((uVar9 >> 0xe & 1) != 0) { 
            //SDL_input._2084_4_ = -iVar2; // 03f2ae30
            *((uint32_t *)(VAR_SDL_INPUT + 2084)) = -iVar2;
        }
        if ((uVar9 >> 0x10 & 1) != 0) { 
            //SDL_input._2080_4_ = -iVar2; // 03f2ae28
            *((uint32_t *)(VAR_SDL_INPUT + 2080)) = -iVar2;
        }
        if ((uVar10 >> 0xd & 1) != 0) { 
            //SDL_input._2084_4_ = iVar2; // 03f2ae30
            *((uint32_t *)(VAR_SDL_INPUT + 2084)) = iVar2;
        }
        if ((uVar10 >> 0xf & 1) != 0) { 
            //SDL_input._2080_4_ = iVar2; // 03f2ae28
            *((uint32_t *)(VAR_SDL_INPUT + 2080)) = iVar2;
        }

        // stylus in vertical mode
        usLayoutRotate = getlayout_rotate(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);
        if ((usLayoutRotate == 90) || (usLayoutRotate == 270)) {
            if (evt.mode == MMIYOO_MOUSE_MODE) {
                if ((ulJoyAxis_Var9 >> 43 & 1) != 0) { 
                    *((uint32_t *)(VAR_SDL_INPUT + 2084)) = -iVar2; //ydelta
                }

                if ((ulJoyAxis_Var9 >> 44 & 1) != 0) {
                    *((uint32_t *)(VAR_SDL_INPUT + 2080)) = iVar2;  //xdelta
                }

                if ((ulJoyAxis_Var10 >> 42 & 1) != 0) {
                    *((uint32_t *)(VAR_SDL_INPUT + 2084)) = iVar2; //ydelta
                }

                if ((ulJoyAxis_Var10 >> 45 & 1) != 0) {
                    *((uint32_t *)(VAR_SDL_INPUT + 2080)) = -iVar2; //xdelta
                }
            }
        }

        goto LAB_0009ab68;
    }
	
	//printf("[trngaje] sdl_platform_get_input:step12\n");
LAB_0009abc8:
// SDL_input._2072_4_ : 03f2ae20
// SDL_input._2076_4_ : 03f2ae24
    //SDL_input._2072_4_ = SDL_input._2072_4_ + SDL_input._2080_4_;
    *((uint32_t *)(VAR_SDL_INPUT + 2072)) = (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2072))) + (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2080)));
    //SDL_input._2076_4_ = SDL_input._2076_4_ + SDL_input._2084_4_;
    *((uint32_t *)(VAR_SDL_INPUT + 2076)) = (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2076))) + (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2084)));
    //if (SDL_input._2072_8_ == 0) {
    if ((uint64_t) (*((uint64_t *)(VAR_SDL_INPUT + 2072))) == 0) {
		//printf("[trngaje] sdl_platform_get_input:step12-1\n");
        //if (SDL_input._2088_4_ == -1) goto LAB_0009ac5c;
        if ((uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2088))) == -1) goto LAB_0009ac5c;
        //if (-1 < (int)(SDL_input._2072_4_ + SDL_input._2064_4_)) goto LAB_0009ac14;
        if (-1 < (int)((uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2072))) + (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2064))))) goto LAB_0009ac14;
LAB_0009adb0:
        //SDL_input._2064_4_ = 0;
        *((uint32_t *)(VAR_SDL_INPUT + 2064)) = 0;
        //if (-1 < (int)(SDL_input._2076_4_ + SDL_input._2068_4_)) goto LAB_0009ac2c;
        if (-1 < (int)((uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2076)))  + (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2068))) )) goto LAB_0009ac2c;
LAB_0009adbc:
        //SDL_input._2068_4_ = 0;
        *((uint32_t *)(VAR_SDL_INPUT + 2068)) = 0;
        //if (SDL_input._2088_4_ != -1) goto LAB_0009ac4c;
        if ((uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2088))) != -1) goto LAB_0009ac4c;
LAB_0009add0:
        //uVar3 = SDL_input._2064_4_;
        uVar3 = (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2064)));
        //uVar6 = SDL_input._2068_4_;
        uVar6 = (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2068)));
        if (*(char *)(param_1 + 0x8001c) != '\0') {
LAB_0009add8:
            *(unsigned int *)(param_1 + 0x80014) = uVar3;
            *(unsigned int *)(param_1 + 0x80018) = uVar6;
        }
    }
    else {
		/*
		int x,y,xdir,ydir,xdelta,ydelta;
		
		x =(uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2064)));
		y=(uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2068)));
		xdir=(uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2072)));
		ydir=(uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2076)));
		xdelta=(uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2080)));
		ydelta=(uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2084)));
		
		printf("[trngaje] x=%d,y=%d,xdir=%d,ydir=%d,xdelta=%d,ydelta=%d\n", x, y, xdir, ydir, xdelta, ydelta);
		*/
		//2064 (4bytes) : x pos 
		//2068 (4bytes) : y pos 
		//2072 (4bytes): x 증감 (1/-1) 
		//2076 (4bytes): y 증감 (1/-1) 
		//2080 (4bytes): x delta 
		//2084 (4bytes): y delta 

		// 터치 입력시 여기로 진입함
        //if ((int)(SDL_input._2072_4_ + SDL_input._2064_4_) < 0) goto LAB_0009adb0;
        if ((int)((uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2072))) + (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2064)))) < 0) goto LAB_0009adb0;
LAB_0009ac14:
        //SDL_input._2064_4_ = SDL_input._2072_4_ + SDL_input._2064_4_;
        *((uint32_t *)(VAR_SDL_INPUT + 2064)) = (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2072))) + (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2064)));
        //if (0xff < (int)SDL_input._2064_4_) {
        if (0xff < (int)(uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2064)))) { // x축 max 값
            //SDL_input._2064_4_ = 0xff;
            *((uint32_t *)(VAR_SDL_INPUT + 2064)) = 0xff;
        }
        //if ((int)(SDL_input._2076_4_ + SDL_input._2068_4_) < 0) goto LAB_0009adbc;
        if ((int)((uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2076))) + (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2068)))) < 0) goto LAB_0009adbc;
LAB_0009ac2c:
        //SDL_input._2068_4_ = SDL_input._2076_4_ + SDL_input._2068_4_;
        *((uint32_t *)(VAR_SDL_INPUT + 2068)) = (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2076))) + (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2068)));
        //if (0xbf < (int)SDL_input._2068_4_) {
        if (0xbf < (int)(uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2068)))) { // y축 max 값
            //SDL_input._2068_4_ = 0xbf;
            *((uint32_t *)(VAR_SDL_INPUT + 2068)) = 0xbf;
        }
        //if (SDL_input._2088_4_ == -1) goto LAB_0009add0;
        if ((uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2088))) == -1) goto LAB_0009add0;
LAB_0009ac4c:
        //uVar6 = SDL_input._2068_4_;
        uVar6 = (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2068)));
        //uVar3 = SDL_input._2064_4_;
        uVar3 = (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2064)));
        //uVar7 = SDL_input._2088_4_ & 0xff;
        uVar7 =  (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2088))) & 0xff;
        //*(char *)(param_1 + 0x8001c) = (char)SDL_input._2088_4_;
        *(char *)(param_1 + 0x8001c) = (char)(uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2088)));
        if (uVar7 != 0) goto LAB_0009add8;
    }
	//printf("[trngaje] sdl_platform_get_input:step13\n");
    screen_set_cursor_position((int)(*((uint32_t *)(VAR_SDL_INPUT + 2064))),  // 한번씩 움직일때만 숫자가 변경됨, 유지하는 경우에는 변경되지 않음
		 (int)(*((uint32_t *)(VAR_SDL_INPUT + 2068))),  // 변화 없음
		 (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2088))) & 0xff);  // log를 통해 변수값 맵핑// parameter 갯수가 안맞음
LAB_0009ac5c:
    *(uint *)(param_1 + 0x80010) = uVar12; // retrun 값

	//printf("[trngaje] sdl_platform_get_input--\n");
    return;
}

int idelta_x_minus=0;
int idelta_x_plus=0;
int idelta_y_minus=0;
int idelta_y_plus=0;

void dpad2stylus(uint uiDpad, uint uiUpdateButton)
{
    uint32_t stylus_x, stylus_y, stylus_touch;
    nds_screen_set_cursor_position screen_set_cursor_position;

    screen_set_cursor_position = (nds_screen_set_cursor_position)FUN_SCREEN_SET_CURSOR_POSITION;
    
    stylus_x = *((uint32_t *)(VAR_SDL_INPUT + 2064));
    stylus_y = *((uint32_t *)(VAR_SDL_INPUT + 2068));

    if ((uiDpad & 0xf0) || (uiUpdateButton != 0)) {
        if (uiDpad & 0x40) {// up
            //stylus_y--;
            stylus_y-=1+idelta_y_minus/20;
            idelta_y_minus++;
            idelta_y_plus=0;
        } else if (uiDpad & 0x80) {// down
            //stylus_y++;
            stylus_y+=1+idelta_y_plus/20;
            idelta_y_plus++;
            idelta_y_minus=0;
        }
        if (uiDpad & 0x20)  {// left
            //stylus_x --;
            stylus_x -=1+idelta_x_minus/20;
            idelta_x_minus++;
            idelta_x_plus=0;
        } else if (uiDpad & 0x10) {// right
            //stylus_x ++;
            stylus_x +=1+idelta_x_plus/20;
            idelta_x_plus++;
            idelta_x_minus=0;
        }

        if (0xff < (int)stylus_x) { // x축 max 값
            stylus_x = 0xff;
        }

        if (0xbf < (int)stylus_y) { // y축 max 값
            stylus_y = 0xbf;
        }
     
        *((uint32_t *)(VAR_SDL_INPUT + 2064)) = stylus_x;
        *((uint32_t *)(VAR_SDL_INPUT + 2068)) = stylus_y; 

        screen_set_cursor_position((int)stylus_x,  (int)stylus_y,  
             (uint32_t) (*((uint32_t *)(VAR_SDL_INPUT + 2088))) & 0xff);  // 이 값은 변경 필요함
    }
    else {
        // center
        idelta_x_minus=0;
        idelta_x_plus=0;
        idelta_y_minus=0;
        idelta_y_plus=0;
    }

}

void process_input_toggle_dpad_mouse(void)
{
    unsigned long frame_count;

    // toggle
    if (evt.mode == MMIYOO_KEYPAD_MODE) {   // dpad mode -> stylus mode
        evt.mode = MMIYOO_MOUSE_MODE;
    }
    else {
        evt.mode =  MMIYOO_KEYPAD_MODE;
        // clear counter to hide the cursor of stylus

        frame_count =   *((uint32_t *)VAR_SDL_FRAME_COUNT);

        if (frame_count > 0) {
            *((uint32_t *)VAR_SDL_FRAME_COUNT) = 0;
        }        
    }
}

/*
up : 0x80000001
down : 0x2
left : 0x4
right : 0x8
a : 0x10
b : 0x20
x : 0x40
y : 0x80
l : 0x100
r : 0x200
start : 0x400
select : 0x800
touch up : 0x2000
touch down : 0x4000
touch left : 0x8000
touch right : 0x10000
touch press: 0x20000 (r3)
menu : 0x40000 menu (l3)
swap_screen: 0x400000 (r2)
orientation: 0x800000 (l2)

fake mic : 0x8000000 (F)
*/

void sdl_update_input(long param_1)
{
    //static uint backup_input=-1;
    static unsigned long backup_input=0;
    
    unsigned int input_value[2];
    nds_get_gui_input get_gui_input;
    int guikey;
       
    uint **ppuVar1;
    uint uVar2;
    uint uVar3;
    byte bVar4;
    int iVar5;
    void *__ptr;
    void *__ptr_00;
    ushort uVar6;
    ulong uVar7;
    ulong uVar8;
    uint uVar9;
    ulong uVar10;
    ulong *puVar11;
    ushort uVar12;
    uint *puVar13;
    unsigned char auStack_828 [2080];

    // add by trngaje
    //ulong inputvalue;
    nds_platform_get_input platform_get_input;
    nds_lua_is_active lua_is_active;
    nds_lua_on_frame_update lua_on_frame_update;
    nds_set_debug_mode set_debug_mode;
    nds_screen_copy16 screen_copy16;
    nds_save_state_index save_state_index;
    nds_load_state_index load_state_index;
    nds_menu menu;
    nds_set_screen_swap set_screen_swap;
    nds_set_screen_orientation set_screen_orientation;
    nds_quit quit;
    nds_spu_fake_microphone_stop  spu_fake_microphone_stop;
    nds_spu_fake_microphone_start spu_fake_microphone_start;
    nds_touchscreen_set_position touchscreen_set_position;
    nds_cpu_block_log_all cpu_block_log_all;

    unsigned char  ucLayoutType;
    unsigned short  usLayoutRotate;

    DPAD_ROTATE dpad_rotate[4] = {{0x40, 0x80, 0x20, 0x10, },  // up, down, left, right 
                                                                        {0x10,0x20,0x40,0x80,},
                                                                        {0x80, 0x40, 0x10, 0x20, },
                                                                        {0x20,0x10,0x80,0x40,}};
    BUTTON_ROTATE button_rotate[4] = {{0x20, 0x10, 0x40, 0x80}, // b,a,x,y
                                                                                {0x10, 0x40, 0x80, 0x20}, 
                                                                                {0x40, 0x80, 0x20, 0x10}, 
                                                                                {0x80, 0x20, 0x10, 0x40}
                                                                            };

    ppuVar1 = (uint **)(param_1 + 0x80000);
    puVar11 = *(ulong **)(param_1 + 0x80008);
    uVar3 = *(uint *)(param_1 + 0x80010);
 
//[trngaje] VAR_SYSTEM=0x47b530
//[trngaje] VAR_SYSTEM_SAVESTATE_NUM=0x48c068
//[trngaje] VAR_SYSTEM_SAVESTATE_NUM=0x5589322ee8
//[trngaje] VAR_SYSTEM=0x558929d528

//[trngaje] VAR_SYSTEM=0x47b528
//[//trngaje] VAR_SYSTEM_SAVESTATE_NUM=0x48c060

   
    //printf("[trngaje] VAR_SYSTEM=%p\n", (uint **)(param_1 + 0x80008 - base_addr_rx));
    //printf("[trngaje] VAR_SYSTEM_SAVESTATE_NUM=%p\n", (uint **)(param_1 + 0x80008 + 0x10b38 - base_addr_rx));
    platform_get_input = (nds_platform_get_input)FUN_PLATFORM_GET_INPUT;
	//printf("[trngaje] VAR_SDL_INPUT + 2072=0x%x\n", *((uint64_t *)(VAR_SDL_INPUT + 2072)) );
    lua_is_active = (nds_lua_is_active)FUN_LUA_IS_ACTIVE;
    lua_on_frame_update = (nds_lua_on_frame_update)FUN_LUA_ON_FRAME_UPDATE;
    set_debug_mode = (nds_set_debug_mode)FUN_SET_DEBUG_MODE;
    screen_copy16 = (nds_screen_copy16)FUN_SCREEN_COPY16;
    save_state_index = (nds_save_state_index)FUN_SAVE_STATE_INDEX;
    load_state_index = (nds_load_state_index)FUN_LOAD_STATE_INDEX;
    menu = (nds_menu)FUN_MENU;
    set_screen_swap = (nds_set_screen_swap)FUN_SET_SCREEN_SWAP;
    set_screen_orientation = (nds_set_screen_orientation)FUN_SET_SCREEN_ORIENTATION;
    quit = (nds_quit)FUN_QUIT;
    spu_fake_microphone_stop = (nds_spu_fake_microphone_stop)FUN_SPU_FAKE_MICROPHONE_STOP;
    spu_fake_microphone_start = (nds_spu_fake_microphone_start)FUN_SPU_FAKE_MICROPHONE_START;
    touchscreen_set_position = (nds_touchscreen_set_position)FUN_TOUCHSCREEN_SET_POSITION;
    cpu_block_log_all = (nds_cpu_block_log_all)FUN_CPU_BLOCK_LOG_ALL;

    get_gui_input = (nds_get_gui_input)FUN_GET_GUI_INPUT;
    
    platform_get_input(param_1);
    
    
    if ((*(int *)((long)puVar11 + 0x85a0c) != 0) && (iVar5 = lua_is_active(), iVar5 != 0)) {
        lua_on_frame_update();
    }
    uVar9 = *(uint *)(param_1 + 0x80010);
    uVar10 = (ulong)uVar9;
    
    if (*(char *)(param_1 + 0x80038) == '\x02') { // if (input->log_mode == '\x02') {
        puVar13 = *ppuVar1;
        if ((ulong)*puVar13 != *puVar11) goto LAB_00098530;
        uVar9 = puVar13[1] & 0x7fffffff;
      
        printf("[trngaje] sdl_update_input(x02): 0x%x\n", uVar9);
        uVar10 = (ulong)uVar9;
        *(byte *)(param_1 + 0x8001c) = (byte)(puVar13[1] >> 0x1f);
        *(uint *)(param_1 + 0x80014) = (uint)*(byte *)(puVar13 + 2);
        bVar4 = *(byte *)((long)puVar13 + 9);
        *ppuVar1 = (uint *)((long)puVar13 + 10);
        *(uint *)(param_1 + 0x80010) = uVar9;
        *(uint *)(param_1 + 0x80018) = (uint)bVar4;
        if ((uVar9 >> 0x1c & 1) == 0) goto LAB_00098534;
LAB_00098818:
        set_debug_mode(puVar11 + 0x2b91cd,0);
        *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xefffffff;
        if (((uint)uVar10 >> 0x1d & 1) == 0) goto LAB_00098538;
LAB_000987f0:
        set_debug_mode(puVar11 + 0x4b9e8b,0);
        *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xdfffffff;
        if (((uint)uVar10 >> 0x1e & 1) == 0) goto LAB_0009853c;
LAB_00098840:
        __sprintf_chk(auStack_828,1,0x820,"%s%cprofiles%c%s_translation_post.txt",puVar11 + 0x113e7,0x2f
                  ,0x2f,puVar11 + 0x11567);
        __printf_chk(1,"Logging recompiled block information to %s.\n",auStack_828);
        cpu_block_log_all(puVar11,auStack_828);
        if (((uint)uVar10 >> 0x13 & 1) == 0) goto LAB_00098540;
LAB_00098780: // save_state
        __ptr = malloc(0x18000);
        __ptr_00 = malloc(0x18000);
        screen_copy16(__ptr,0);
        screen_copy16(__ptr_00,1);
        save_state_index(puVar11,*(unsigned int *)(puVar11 + 0x10b38),__ptr,__ptr_00);
        free(__ptr);
        free(__ptr_00);
        *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xfff7ffff;
        if (((uint)uVar10 >> 0x14 & 1) == 0) goto LAB_00098544;
LAB_000986e4: // load state
        uVar8 = puVar11[0x4b9eac];
        uVar7 = puVar11[0x2b91ee];
        *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xffefffff;
        __printf_chk(1,"load state @ %lx, %lx in.\n",uVar7,uVar8);
        if (*(char *)((long)puVar11 + 0x15c8f99) == '\a') {
            set_debug_mode(puVar11 + 0x2b91cd,0);
        }
        if (*(char *)((long)puVar11 + 0x25cf589) == '\a') {
            set_debug_mode(puVar11 + 0x4b9e8b,0);
        }
        load_state_index(puVar11,*(unsigned int *)(puVar11 + 0x10b38),0,0,0);
        //if (iVar5 == 0) goto LAB_000986b4;
        uVar9 = (uint)uVar10;
    }
    else { // 버튼을 누르면 주로 여기로 진입함
        //printf("[trngaje] sdl_update_input(else): 0x%x\n", uVar9);
        if (((*(uint *)(param_1 + 0x80020) != uVar9) || //  if (((uVar8 != input->last_button_status) ||
            (*(char *)(param_1 + 0x8002c) != *(char *)(param_1 + 0x8001c))) || // (input->last_touch_status != input->touch_status)) ||
            ((*(char *)(param_1 + 0x8002c) != '\0' && // ((input->last_touch_status != '\0' &&
            ((*(int *)(param_1 + 0x80014) != *(int *)(param_1 + 0x80024) || // ((input->touch_x != input->last_touch_x || (input->touch_y != input->last_touch_y)))))) {
            (*(int *)(param_1 + 0x80018) != *(int *)(param_1 + 0x80028))))))) {
            puVar13 = *ppuVar1;
            if (puVar13 < (uint *)(param_1 + 0x7ffecU)) { // if (__ptr < input->capture_buffer + 0x7ffec) {
                *puVar13 = (uint)*puVar11; // *__ptr = *(undefined4 *)&system_00->frame_number;
                puVar13[1] = uVar9 & 0x7fffffff | (uint)*(byte *)(param_1 + 0x8001c) << 0x1f; // __ptr[1] = uVar8 & 0x7fffffff | (uint)input->touch_status << 0x1f;
                *(char *)(puVar13 + 2) = (char)*(unsigned int *)(param_1 + 0x80014); // *(u8 *)(__ptr + 2) = (u8)input->touch_x;
                *(char *)((long)puVar13 + 9) = (char)*(unsigned int *)(param_1 + 0x80018); // *(u8 *)((int)__ptr + 9) = (u8)input->touch_y;
                if (*(long *)(param_1 + 0x80030) != 0) { // if ((FILE *)input->log_file != (FILE *)0x0) {
                    __printf_chk(1,"input capture button to log %p\n");
                    fwrite(puVar13,10,1,*(FILE **)(param_1 + 0x80030));
                    fflush(*(FILE **)(param_1 + 0x80030));
                }
                *ppuVar1 = (uint *)((long)puVar13 + 10);
            }
            *(unsigned char *)(param_1 + 0x8002c) = *(unsigned char *)(param_1 + 0x8001c); // input->last_button_status = uVar8;
            *(uint *)(param_1 + 0x80020) = uVar9; // input->last_touch_x = input->touch_x;
            *(unsigned int *)(param_1 + 0x80024) = *(unsigned int *)(param_1 + 0x80014); // input->last_touch_y = input->touch_y;
            *(unsigned int *)(param_1 + 0x80028) = *(unsigned int *)(param_1 + 0x80018); // input->last_touch_status = input->touch_status;
        }

    
LAB_00098530:
        if ((uVar9 >> 0x1c & 1) != 0) goto LAB_00098818; // set_debug_mode
LAB_00098534:
        if (((uint)uVar10 >> 0x1d & 1) != 0) goto LAB_000987f0; // set_debug_mode
LAB_00098538:
        if (((uint)uVar10 >> 0x1e & 1) != 0) goto LAB_00098840; // cpu_block_log_all

LAB_0009853c:
        if ((g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_ASSIGN_HOT))) == 0) { 
            if (((uint)uVar10 >> 0x13 & 1) != 0) goto LAB_00098780; // save states
        }
LAB_00098540:
        if ((g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_ASSIGN_HOT))) == 0) { 
            if (((uint)uVar10 >> 0x14 & 1) != 0) goto LAB_000986e4; // load states
        }
LAB_00098544:
        uVar9 = (uint)uVar10;
    }

    if ((g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_ASSIGN_HOT))) == 0) { 
        if ((uVar9 >> 0x12 & 1) != 0) { // enter menu
            nds.menu.drastic.enable = 1;
            *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xfffbffff; // input->button_status = input->button_status & 0xfffbffff;
            g_advdrastic.ulInput  = 0;
            menu(puVar11,0);
            nds.menu.drastic.enable = 0;
            goto LAB_000986b4;
        }
        uVar9 = (uint)uVar10; // load new game
        if ((uVar9 >> 0x19 & 1) != 0) {
            *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xfdffffff;
            menu(puVar11,1);
            goto LAB_000986b4;
        }
        if ((uVar9 >> 0x15 & 1) != 0) { // toggle fast forward
            iVar5 = *(int *)((long)puVar11 + 0x859c4);
            *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xffdfffff;
            if (iVar5 == 0) {
                *(unsigned char *)((long)puVar11 + 0x3b299a3) = 1;
                *(unsigned int *)((long)puVar11 + 0x859c4) = 1;
            }
            else {
                *(unsigned int *)((long)puVar11 + 0x859c4) = 0;
            }
        }
        if ((uVar9 >> 0x16 & 1) != 0) { // swap screens
            uVar2 = *(uint *)((long)puVar11 + 0x859bc) ^ 1;
            *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xffbfffff;
            *(uint *)((long)puVar11 + 0x859bc) = uVar2;
            set_screen_swap(uVar2);
        }
        if ((uVar9 >> 0x17 & 1) != 0) { // swap orientation A
            uVar2 = *(uint *)((long)puVar11 + 0x859b4) ^ 1;
            *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xff7fffff;
            *(uint *)((long)puVar11 + 0x859b4) = uVar2;
            set_screen_orientation(uVar2);
        }
        if ((uVar9 >> 0x18 & 1) != 0) { // swap orientation B
            uVar2 = *(uint *)((long)puVar11 + 0x859b4) ^ 2;
            *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xfeffffff;
            *(uint *)((long)puVar11 + 0x859b4) = uVar2;
            set_screen_orientation(uVar2);
        }
        if ((uVar9 >> 0x1a & 1) != 0) { // quit
            /* WARNING: Subroutine does not return */
            quit(*(unsigned long *)(param_1 + 0x80008));
        }
      uVar2 = uVar3 & 0x8000000; // fake microphone
        if ((uVar9 >> 0x1b & 1) == 0) {
            if (uVar2 != 0) {
                spu_fake_microphone_stop(puVar11 + 0x2b0c00);
            }
        }
        else if (uVar2 == 0) {
            spu_fake_microphone_start(puVar11 + 0x2b0c00);
        }
    }

    // add by trngaje
/*
        if  (g_advdrastic.ulInput != 0) {
            // 0x20000000000, 42
            //printf("[trngaje] %p \n", 1L << (0x29 + ADVANCE_CONTROL_INDEX_ASSIGN_HOT)); 

            
            //if (g_advdrastic.ulInput  & 0x20000000000) {
            //    printf("[trngaje] sdl_update_input(else): hot key pressed\n");
            //}
            printf("[trngaje] sdl_update_input(else): %p\n", g_advdrastic.ulInput);
        }
*/
    //if (uVar9 & 0x800) { // hotkey : select
    if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_ASSIGN_HOT))) { 
        if (g_advdrastic.ulInput != backup_input) {
            if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_CHANGE_LAYOUT_DEC))) {
                if (g_advdrastic.ucLayoutIndex[nds.hres_mode] > 0) {
                    g_advdrastic.ucLayoutIndex[nds.hres_mode] -= 1;
                }
/*
                if (nds.hres_mode == 0) {
                    if (nds.dis_mode > 0) {
                        nds.dis_mode -= 1;
                    }
                }
                else {
                    nds.dis_mode = NDS_DIS_MODE_HRES0;
                }
*/
                backup_input = g_advdrastic.ulInput;
                goto LAB_000986b4;	
            } 
            else if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_CHANGE_LAYOUT_INC))) {
                if (g_advdrastic.ucLayoutIndex[nds.hres_mode] < (getmax_layout(nds.hres_mode)-1)) {
                    g_advdrastic.ucLayoutIndex[nds.hres_mode] += 1;
                }
/*
                if (nds.hres_mode == 0) {
                    //if (nds.dis_mode < NDS_DIS_MODE_LAST) {
                    if (nds.dis_mode < getmax_layout(nds.hres_mode)) {
                        nds.dis_mode += 1;
                    }
                }
                else {
                    nds.dis_mode = NDS_DIS_MODE_HRES1;
                }
*/
                backup_input = g_advdrastic.ulInput;   
                goto LAB_000986b4; 	
            }
            else if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_HOT_CUSTOM_SETTING))) {
                g_advdrastic.ulInput = 0;
                backup_input = g_advdrastic.ulInput;

                display_custom_setting();
                
                *(uint *)(param_1 + 0x80010)  = 0;
                *(uint *)(param_1 + 0x80020) = 0;
/*
                *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xfffffbff;
                *(uint *)(param_1 + 0x80010) = *(uint *)(param_1 + 0x80010) & 0xfffff7ff;

                *(uint *)(param_1 + 0x80020) = *(uint *)(param_1 + 0x80010) & 0xfffffbff;
                *(uint *)(param_1 + 0x80020) = *(uint *)(param_1 + 0x80010) & 0xfffff7ff;              
*/
                goto LAB_000986b4;
            }
            else if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_HOT_TOGGLE_BLUR_PIXEL))) {
                pixel_filter = pixel_filter ? 0 : 1;
                backup_input = g_advdrastic.ulInput;   
                goto LAB_000986b4;
            }
            else if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_HOT_CHANGE_THEME))) {
                ucLayoutType = getlayout_type(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);
                if (evt.mode == MMIYOO_KEYPAD_MODE) {
                    if (/*(nds.overlay.sel >= nds.overlay.max) && */
                        (ucLayoutType != LAYOUT_TYPE_TRANSPARENT) &&
                        (ucLayoutType != LAYOUT_TYPE_HIGHRESOLUTION) &&
                        (ucLayoutType != LAYOUT_TYPE_SINGLE) 
  /*                      (nds.dis_mode != NDS_DIS_MODE_VH_T0) &&
                        (nds.dis_mode != NDS_DIS_MODE_VH_T1) &&
                        (nds.dis_mode != NDS_DIS_MODE_S1) &&
                        (nds.dis_mode != NDS_DIS_MODE_HRES1) */
                        ) {
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
            
                backup_input = g_advdrastic.ulInput;
                goto LAB_000986b4;
            }
            else if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_HOT_SAVE_STATE))) {
                nds.state|= NDS_STATE_QSAVE;
                backup_input = g_advdrastic.ulInput;
                g_advdrastic.ulInput = 0;
                goto LAB_00098780; 
            }
            else if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_HOT_LOAD_STATE))) {
                nds.state|= NDS_STATE_QLOAD;
                backup_input = g_advdrastic.ulInput;
                g_advdrastic.ulInput = 0;
                goto LAB_000986e4; 
            }
            else if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_HOT_QUIT))) {
                quit(*(unsigned long *)(param_1 + 0x80008));
            }
            else if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_HOT_TOGGLE_DPAD_MOUSE))) {
                backup_input = g_advdrastic.ulInput;
                g_advdrastic.ulInput = 0;
                //evt.mode = (evt.mode == MMIYOO_KEYPAD_MODE) ? MMIYOO_MOUSE_MODE : MMIYOO_KEYPAD_MODE;
                process_input_toggle_dpad_mouse();
            }
            else if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_HOT_ENTER_MENU))) {
                nds.menu.drastic.enable = 1;
                g_advdrastic.ulInput  = 0;
                menu(puVar11,0);
                nds.menu.drastic.enable = 0;
                goto LAB_000986b4;               
            }
            else if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_HOT_TOGGLE_FAST_FORWARD))) {
                iVar5 = *(int *)((long)puVar11 + 0x859c4);
                if (iVar5 == 0) {
                    *(unsigned char *)((long)puVar11 + 0x3b299a3) = 1;
                    *(unsigned int *)((long)puVar11 + 0x859c4) = 1;
                }
                else {
                    *(unsigned int *)((long)puVar11 + 0x859c4) = 0;
                }                
            }
            backup_input = g_advdrastic.ulInput;    
        }
    }
    else {
        // without hotkey
        if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_TOGGLE_DPAD_MOUSE))) {
            g_advdrastic.ulInput = 0;
            //evt.mode = (evt.mode == MMIYOO_KEYPAD_MODE) ? MMIYOO_MOUSE_MODE : MMIYOO_KEYPAD_MODE;
            process_input_toggle_dpad_mouse();
        }
        
    }
  
    //printf("[trngaje] uVar10=0x%x\n", uVar10);

    //uVar2 = (uint)(uVar10 >> 4) & 1; // a  0x01
    if (uVar10 & button_rotate[g_advdrastic.uiRotateButtons].ucA/*0x10*/) // a
        uVar2 = 1;
    else
        uVar2 = 0;

    uVar9 = uVar2 | 2;
    if ((uVar10 & button_rotate[g_advdrastic.uiRotateButtons].ucB/*0x20*/) == 0) { // b 0x02
        uVar9 = uVar2;
    }
    
    // ----------------------------------------
    // dpad control
    if (g_advdrastic.uiRotateDPAD < 0 || g_advdrastic.uiRotateDPAD > 3) {
        g_advdrastic.uiRotateDPAD = 0;
    }
      
    uVar2 = uVar9 | dpad_rotate[g_advdrastic.uiRotateDPAD].ucUp; //0x40;
    if ((uVar10 & 1) == 0) { // up
        uVar2 = uVar9;
    }
    uVar9 = uVar2 | dpad_rotate[g_advdrastic.uiRotateDPAD].ucDown; //0x80;
    if ((uVar10 & 2) == 0) { // down
        uVar9 = uVar2;
    }
    uVar2 = uVar9 | dpad_rotate[g_advdrastic.uiRotateDPAD].ucLeft;//0x20;
    if ((uVar10 & 4) == 0) { // left
        uVar2 = uVar9;
    }
    uVar9 = uVar2 | dpad_rotate[g_advdrastic.uiRotateDPAD].ucRight; //0x10;
    if ((uVar10 & 8) == 0) { // right
        uVar9 = uVar2;
    }
    
    // dpad in vertical mode
    usLayoutRotate = getlayout_rotate(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);
    if (((usLayoutRotate == 90) || (usLayoutRotate == 270)) && (evt.mode != MMIYOO_MOUSE_MODE)){
        if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_VERTICAL_UP))) {
            uVar9 |= 0x40;
        }
        else if  (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_VERTICAL_DOWN))) {
            uVar9 |= 0x80;
        }

        if (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_VERTICAL_LEFT))) {
            uVar9 |= 0x20;
        }
        else if  (g_advdrastic.ulInput & (1L << (0x29 + ADVANCE_CONTROL_INDEX_VERTICAL_RIGHT))) {
            uVar9 |= 0x10;
        }
    }

    if (evt.mode == MMIYOO_MOUSE_MODE)
    {
        static int mouse_pressed=0xff;
/*
        if ((usLayoutRotate == 90) || (usLayoutRotate == 270)) {
            if (uVar9 & 0x1) {  // pressed a button
                if (mouse_pressed != 1) {
                    *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 1;
                    *(char *)(param_1 + 0x8001c) = 1;
                    mouse_pressed = 1;
                }

                *(unsigned int *)(param_1 + 0x80014) = *((uint32_t *)(VAR_SDL_INPUT + 2064));
                *(unsigned int *)(param_1 + 0x80018) = *((uint32_t *)(VAR_SDL_INPUT + 2068));
            }
            else {
                if (mouse_pressed) {
                    mouse_pressed = 0;
                    *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 0;
                    *(char *)(param_1 + 0x8001c) = 0;
                    *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 0xff;
                }
            }
        }
        else { */
            if (uVar9 & 0x1) {  // pressed a button
                if (mouse_pressed != 1) {
                    *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 1;
                    *(char *)(param_1 + 0x8001c) = 1;
                    mouse_pressed = 1;
                    dpad2stylus(uVar9, 1);
                }

                *(unsigned int *)(param_1 + 0x80014) = *((uint32_t *)(VAR_SDL_INPUT + 2064));
                *(unsigned int *)(param_1 + 0x80018) = *((uint32_t *)(VAR_SDL_INPUT + 2068));
            }
            else {

                if (mouse_pressed) {
                    mouse_pressed = 0;
                    *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 0;
                    *(char *)(param_1 + 0x8001c) = 0;
                    dpad2stylus(uVar9, 1);
                    *((uint32_t *)(VAR_SDL_INPUT + 2088)) = 0xff;
                }
            }
        //}

            dpad2stylus(uVar9, 0);
        //}
        uVar9 &= 0xfffffffe;   // mask for a button 
        uVar9 &= 0xffffff0f;    // mask for dpad/
    }
     
    // ----------------------------------------
    uVar2 = uVar9 | 0x200;
    if ((uVar10 & 0x100) == 0) { // l
        uVar2 = uVar9;
    }
    uVar9 = uVar2 | 0x100;
    if ((uVar10 & 0x200) == 0) { // r
        uVar9 = uVar2;
    }
    uVar2 = uVar9 | 8;
    if ((uVar10 & 0x400) == 0) { // start
        uVar2 = uVar9;
    }
    uVar9 = uVar2 | 4;
    if ((uVar10 & 0x800) == 0) { // select
        uVar9 = uVar2;
    }

    uVar12 = 0xff03;
    uVar6 = 0xff01;
    if ((uVar10 & button_rotate[g_advdrastic.uiRotateButtons].ucX/*0x40*/) == 0) { // x
        uVar12 = 0xff02;
        uVar6 = 0xff00;
    }

    if ((uVar10 & button_rotate[g_advdrastic.uiRotateButtons].ucY/*0x80*/) != 0) { // y
        uVar6 = uVar12;
    }
    uVar12 = uVar6 | 0x80;
    if ((uVar10 & 0x1000) != 0) {
        uVar12 = uVar6;
    }
  
  //printf("[trngaje] sdl_update_input: uVar9 = 0x%x, uVar12=0x%x\n", uVar9, uVar12);
  
  if (*(char *)(param_1 + 0x8001c) != '\0') {
    // 터치 입력시 여기로 진입함
    //printf("[trngaje] here=0x%x\n", *(char *)(param_1 + 0x8001c));
    uVar12 = uVar12 | 0x40;
    touchscreen_set_position
              (puVar11 + 0xa9a,*(unsigned int *)(param_1 + 0x80014),*(unsigned int *)(param_1 + 0x80018)
              );
  }
  uVar6 = *(ushort *)((long)puVar11 + 0x35eead2);
  if ((uVar6 >> 0xe & 1) != 0) {
    if ((short)uVar6 < 0) {
      if ((uVar6 & uVar9) != 0) goto LAB_00098aa4;
    }
    else if ((uVar6 & uVar9) == uVar9) {
LAB_00098aa4:
      uVar7 = puVar11[0x2b91ba];
      uVar2 = *(uint *)(uVar7 + 0x214) | 0x1000;
      *(uint *)(uVar7 + 0x214) = uVar2;
      if ((*(uint *)(puVar11 + 0x2b91cc) >> 2 & 1) == 0) {
        *(uint *)(puVar11 + 0x2b91cb) = -*(int *)(uVar7 + 0x208) & uVar2 & *(uint *)(uVar7 + 0x210);
      }
    }
  }
  uVar6 = *(ushort *)((long)puVar11 + 0x35f6ad2);
  if ((uVar6 >> 0xe & 1) != 0) {
    if ((short)uVar6 < 0) {
      if ((uVar6 & uVar9) != 0) goto LAB_00098a54;
    }
    else if ((uVar6 & uVar9) == uVar9) {
LAB_00098a54:
      uVar7 = puVar11[0x4b9e78];
      uVar2 = *(uint *)(uVar7 + 0x214) | 0x1000;
      *(uint *)(uVar7 + 0x214) = uVar2;
      if ((*(uint *)(puVar11 + 0x4b9e8a) >> 2 & 1) == 0) {
        *(uint *)(puVar11 + 0x4b9e89) = -*(int *)(uVar7 + 0x208) & uVar2 & *(uint *)(uVar7 + 0x210);
      }
    }
  }
  uVar6 = (ushort)uVar9 ^ 0x3ff;
  *(ushort *)(puVar11 + 0x6bdd5a) = uVar6;
  *(ushort *)(puVar11 + 0x6bed5a) = uVar6;
  *(ushort *)((long)puVar11 + 0x35f6ad6) = ~uVar12;
  if ((uVar3 & 0x1000) != 0 && (uVar10 & 0x1000) == 0) {
    uVar10 = puVar11[0x4b9e78];
    uVar3 = *(uint *)(uVar10 + 0x214) | 0x400000;
    *(uint *)(uVar10 + 0x214) = uVar3;
    if ((*(uint *)(puVar11 + 0x4b9e8a) >> 2 & 1) == 0) {
      *(uint *)(puVar11 + 0x4b9e89) = -*(int *)(uVar10 + 0x208) & uVar3 & *(uint *)(uVar10 + 0x210);
    }
  }
LAB_000986b4:


  return;
}

// [trngaje] save_state_num=0x47b9c0
void sdl_draw_menu_bg(unsigned long *param_1) // draw_menu_bg(&local_578); 이라 어드레스 접근이 어려움
{
    void *__s;
    void *__s_00;
    char *pcVar1;
    unsigned int uVar2;
    long local_10;

    nds_savestate_index_timestamp savestate_index_timestamp;
    
    //printf("[trngaje] sdl_draw_menu_bg:param_1=%p\n", param_1 - base_addr_rx);
    //printf("[trngaje] save_state_num=%p\n", param_1[1] + 0x458 - base_addr_rx);
    printf("[trngaje] 0x%x\n", *(unsigned int *)(param_1[1] + 0x458));
    g_save_slot = *(unsigned int *)(param_1[1] + 0x458);


    savestate_index_timestamp = (nds_savestate_index_timestamp)FUN_SAVESTATE_INDEX_TIMESTAMP;

  //clear_screen_menu(0,&__stack_chk_guard,0);
  if (param_1[7] != 0) {
    uVar2 = 200;
    if (*(int *)(param_1 + 8) != 0) {
      uVar2 = 0x24;
    }
    //blit_screen_menu(param_1[7],uVar2,0x28,400,0x96);
  }
  uVar2 = 0x160;
  if (*(int *)(param_1 + 8) == 0) {
    uVar2 = 0x204;
  }
  //set_font_narrow_small();
  sdl_print_string("Version r2.5.2.0",0xffff,0,uVar2,0xc9);
  //set_font_wide();
  if (*(int *)(param_1 + 8) != 0) {
    if ((*(long *)(param_1[2] + 0x28) == 0) && (*(int *)(param_1[2] + 0x18) == 5)) {
      local_10 = savestate_index_timestamp(*param_1,*(unsigned int *)(param_1[1] + 0x458)); // 이 함수 리턴값 없음, 32bit drastic 확인할 것
      //set_font_narrow_small();
      if (local_10 == 0) {
        sdl_print_string("(No savestate)",0xffff,0,0x220,0xe9);
        //set_font_wide();
      }
      else {
        //__s = malloc(0x18000);
        //__s_00 = malloc(0x18000);
        pcVar1 = ctime(&local_10);
       // memset(__s,0,0x18000);
        //memset(__s_00,0,0x18000);
        //load_state_index(*param_1,*(undefined4 *)(param_1[1] + 0x458),__s,__s_00,1);
        //blit_screen_menu(__s,0x1d8,0x30,0x100,0xc0);
        //blit_screen_menu(__s_00,0x1d8,0xf0,0x100,0xc0);
        sdl_print_string(pcVar1,0xffff,0,0x1dc,0x19c);
        //free(__s);
        //free(__s_00);
        //set_font_wide();
      }
    }
    else {
      //blit_screen_menu(param_1[5],0x1d8,0x30,0x100,0xc0);
      //blit_screen_menu(param_1[6],0x1d8,0xf0,0x100,0xc0);
    }
  }
      
}

void sdl_quit(long param_1)
{
    nds_gamecard_close gamecard_close;
    nds_audio_exit audio_exit;
    nds_input_log_close input_log_close;
    nds_uninitialize_memory uninitialize_memory;
    nds_platform_quit platform_quit;
    nds_save_directory_config_file save_directory_config_file;
    
    gamecard_close = (nds_gamecard_close)FUN_GAMECARD_CLOSE;
    audio_exit = (nds_audio_exit)FUN_AUDIO_EXIT;
    input_log_close = (nds_input_log_close)FUN_INPUT_LOG_CLOSE;
    uninitialize_memory = (nds_uninitialize_memory)FUN_UNINITIALIZE_MEMORY;
    platform_quit = (nds_platform_quit)FUN_PLATFORM_QUIT;
    save_directory_config_file = (nds_save_directory_config_file)FUN_SAVE_DIRECTORY_CONFIG_FILE;
    
    printf("[trngaje]sdl_quit: param_1=0x%x\n", param_1);
    if (g_advdrastic.uiEnableautosavestate > 0) {
        dtr_savestate(g_advdrastic.uiSlotOfAutosavestate);
    }
    drastic_VideoQuit();
  /*  
    if (nds_system[param_1 + 0x37339a8] == '\0') {
      cpu_print_profiler_results();
    }
    __printf_chk(((double)(unkuint9)(mini_hash_accesses - mini_hash_misses) * 100.0) /
               (double)(unkuint9)mini_hash_accesses,1,
               "%lu mini hash hits out of %lu accesses (%lf%%)\n",
               mini_hash_accesses - mini_hash_misses);
    __printf_chk(1,"%lu hash accesses:\n",hash_accesses);
    __printf_chk(((double)(unkuint9)hash_hit_lengths * 100.0) / (double)(unkuint9)hash_accesses,1,
               " %lf%% hit in one hop\n");
    __printf_chk(((double)(unkuint9)DAT_00164018 * 100.0) / (double)(unkuint9)hash_accesses,1,
               " %lf%% hit in two hops\n");
    __printf_chk(((double)(unkuint9)DAT_00164020 * 100.0) / (double)(unkuint9)hash_accesses,1,
               " %lf%% hit in three hops\n");
    __printf_chk(((double)(unkuint9)DAT_00164028 * 100.0) / (double)(unkuint9)hash_accesses,1,
               " %lf%% hit in four or more hops\n");
               */
    save_directory_config_file(param_1,"drastic.cf2");
    if (*(char *)(param_1 + 0x8ab38) != '\0') {
        printf("[trngaje] pre gamecard_close\n");
        gamecard_close(param_1 + 800);
    }
    audio_exit(param_1 + 0x1586000);
    input_log_close(param_1 + 0x5528);
    uninitialize_memory(param_1 + 0x35d3930);
    platform_quit();
    //fflush(_stdout);
                    /* WARNING: Subroutine does not return */
    exit(0);
}

// 여긴 계속 호출되고 있음
void sdl_update_screens(void)
{
    static int call_count = 0;
    if (call_count < 5) {
        printf("[DEBUG] sdl_update_screens called (count=%d), base_addr_rx=0x%lx\n", call_count, base_addr_rx);
        call_count++;
    }
    
    // acquire a lock
    pthread_mutex_lock(&lock);
    
    nds.update_screen = 1;
    pthread_cond_signal(&request_update_screen_cond);
    
    pthread_cond_wait(&response_update_screen_cond, &lock);
    
    // release lock 
    pthread_mutex_unlock(&lock);     
}

void sdl_update_screen(void)
{
}

void sdl_print_string(char *p, uint32_t fg, uint32_t bg, uint32_t x, uint32_t y)
{
    int w = 0, h = 0;
    SDL_Color col = {0};
    SDL_Surface *t0 = NULL;
    SDL_Surface *t1 = NULL;
    static int fps_cnt = 0;

    printf(PREFIX"sdl_print_string:p=%s, fg=0x%x, bg=0x%x, x=%d, y=%d\n", p, fg, bg, x, y);

    if (p && (strlen(p) > 0)) {
        if (drastic_menu.cnt < MAX_MENU_LINE) {
            drastic_menu.item[drastic_menu.cnt].x = x;
            drastic_menu.item[drastic_menu.cnt].y = y;
            drastic_menu.item[drastic_menu.cnt].fg = fg;
            drastic_menu.item[drastic_menu.cnt].bg = bg;
            strcpy(drastic_menu.item[drastic_menu.cnt].msg, p);
            //printf(PREFIX"sdl_print_string:cnt=%d, x:%d, y:%d, fg:0x%x, bg:0x%x, \'%s\'\n", drastic_menu.cnt, x, y, fg, bg, p);
            drastic_menu.cnt+= 1;
        }
        //printf(PREFIX"x:%d, y:%d, fg:0x%x, bg:0x%x, \'%s\'\n", x, y, fg, bg, p);

    }

    if ((x == 0) && (y == 0) && (fg == 0xffff) && (bg == 0x0000)) {
        if (fps_cnt++ > 60) {
            fps_cnt = 0;

            w = strlen(p);
            for (h=w-1; h>=0; h--) {
                if (p[h] == ' ') {
                    p[h] = 0;
                    break;
                }
            }

            col.r = 0xcc;
            col.g = 0xcc;
            col.b = 0x00;
            TTF_SizeUTF8(nds.font, p, &w, &h);
            t0 = TTF_RenderUTF8_Solid(nds.font, p, col);
            if (t0) {
                t1 = SDL_CreateRGBSurface(SDL_SWSURFACE, t0->w, t0->h, 32, 0, 0, 0, 0);
                if (t1) {
                    SDL_FillRect(t1, &t1->clip_rect, 0x000000);
                    SDL_BlitSurface(t0, NULL, t1, NULL);

                    if (fps_info) {
                        SDL_FreeSurface(fps_info);
                    }
                    fps_info = SDL_ConvertSurface(t1, cvt->format, 0);
                    SDL_FreeSurface(t1);
                }
                SDL_FreeSurface(t0);
            }
            show_fps = 1;
        }
    }
}

/*
32bit drastic 에서 정의되어 있는 값
struct config_struct {
    struct config_firmware_struct firmware; // 0 
    char rom_directory[1024]; 		// 0x3c
    u32 file_list_display_type; 		// 0x43c
    u32 frameskip_type; 					// 0x440 w0,[x22, #0x5a8] *(int *)(param_1 + 0x859a8) = (int)lVar6; // base 0x85400 w0,[x22, #0x5ac] 
    u32 frameskip_value; 				// 0x444 w0,[x22, #0x5ac] *(int *)(param_1 + 0x859ac) = (int)lVar6;
    u32 show_frame_counter; 		// 0x 448
    u32 screen_orientation;			// 0x44c
    u32 screen_scaling;						// 0x450
    u32 screen_swap;						// 0x454
    u32 savestate_number;				// 0x458 
    u32 fast_forward;							// 0x45c
    u32 enable_sound;						// 0x460
    u32 clock_speed;							// 0x464
    u32 threaded_3d;							// 0x468
    u32 mirror_touch;						// 0x46c
    u32 compress_savestates;		// 0x470
    u32 savestate_snapshot;			// 0x474
    u32 enable_cheats;						// 0x478
    u32 unzip_roms;							// 0x47c
    u32 backup_in_savestates;		// 0x480
    u32 ignore_gamecard_limit;	// 0x484
    u32 frame_interval;						// 0x488
    u32 batch_threads_3d_count;// 0x48c
    u32 trim_roms;								// 0x490
    u32 fix_main_2d_screen;			// 0x494
    u32 disable_edge_marking;		// 0x498
    u32 hires_3d;									// 0x49c
    u32 bypass_3d;								// 0x4a0
    u32 enable_lua;							// 0x4a4
    u32 use_rtc_custom_time;		// 0x4a8
    time_t rtc_custom_time;			// 0x4ac
    u32 rtc_system_time;					// 0x4b0
    u16 controls_a[40];						// 0x4b4
    u16 controls_b[40];						// 0x504
    u64 controls_code_to_config_map[2048];	// 0x558
};
*/

/* [trngaje] input_a(0):0x1
[trngaje] input_b(0):0x1
[trngaje] input_a(1):0x2
[trngaje] input_b(1):0x2
[trngaje] input_a(2):0x4
[trngaje] input_b(2):0x4
[trngaje] input_a(3):0x8
[trngaje] input_b(3):0x8
[trngaje] input_a(4):0x10
[trngaje] input_b(4):0x10
[trngaje] input_a(5):0x20
[trngaje] input_b(5):0x20
[trngaje] input_a(6):0x40
[trngaje] input_b(6):0x40
[trngaje] input_a(7):0x80
[trngaje] input_b(7):0x80
[trngaje] input_a(8):0x100
[trngaje] input_b(8):0x100
[trngaje] input_a(9):0x200
[trngaje] input_b(9):0x200
[trngaje] input_a(10):0x400
[trngaje] input_b(10):0x400
[trngaje] input_a(11):0x800
[trngaje] input_b(11):0x800
[trngaje] input_b(13):0x2000
[trngaje] input_b(14):0x4000
[trngaje] input_b(15):0x8000
[trngaje] input_b(16):0x10000
[trngaje] input_b(17):0x20000
[trngaje] input_a(18):0x40000
[trngaje] input_b(18):0x40000
[trngaje] input_a(19):0x80000
[trngaje] input_a(20):0x100000
[trngaje] input_a(21):0x200000
[trngaje] input_a(22):0x400000
[trngaje] input_b(22):0x400000
[trngaje] input_b(23):0x800000
[trngaje] input_a(26):0x4000000
[trngaje] input_b(27):0x8000000
[trngaje] input_a(28):0x10000000
[trngaje] input_a(30):0x40000000
[trngaje] input_a(31):0x80000001
[trngaje] input_b(31):0x80000001
[trngaje] input_a(32):0x100000002
[trngaje] input_b(32):0x100000002
[trngaje] input_a(33):0x200000004
[trngaje] input_b(33):0x200000004
[trngaje] input_a(34):0x400000008
[trngaje] input_b(34):0x400000008
[trngaje] input_a(35):0x800000010
[trngaje] input_b(35):0x800000010
[trngaje] input_a(36):0x1000000020
[trngaje] input_b(36):0x1000000020
[trngaje] input_a(37):0x2000400000
[trngaje] input_b(37):0x2000000040
[trngaje] input_a(38):0x4000000100
[trngaje] input_b(38):0x4000000100
[trngaje] input_a(39):0x8000000200
[trngaje] input_b(39):0x8000000200
[trngaje] input_a(40):0x10000000040
[trngaje] input_b(40):0x10000000080

*/
void sdl_load_config_file(long param_1, unsigned long param_2, unsigned int param_3) // s32 load_config_file(system_struct *system,char *file_name,u32 game_specific)
{
	void *__s;
	ushort uVar1;
	int iVar2;
	FILE *__stream;
	char *pcVar3;
	ulong uVar4;
	unsigned long uVar5;
	char *__s1;
	long lVar6;
	char acStack_838 [4];
	unsigned char auStack_834 [4];
	unsigned char auStack_830 [8];
	char acStack_828 [1024];
	char acStack_428 [1056];

	nds_load_config_file_binary load_config_file_binary;
	nds_save_config_file save_config_file;
	nds_chomp_whitespace chomp_whitespace;
	nds_set_screen_scale_factor set_screen_scale_factor;
	nds_set_screen_swap set_screen_swap;
	nds_set_screen_orientation set_screen_orientation;
	nds_skip_whitespace skip_whitespace;
	
	load_config_file_binary = (nds_load_config_file_binary)FUN_LOAD_CONFIG_FILE_BINARY;
	save_config_file = (nds_save_config_file)FUN_SAVE_CONFIG_FILE;
	chomp_whitespace = (nds_chomp_whitespace)FUN_CHOMP_WHITESPACE;
	set_screen_scale_factor = (nds_set_screen_scale_factor)FUN_SET_SCREEN_SCALE_FACTOR;
	set_screen_swap = (nds_set_screen_swap)FUN_SET_SCREEN_SWAP;
	set_screen_orientation = (nds_set_screen_orientation)FUN_SET_SCREEN_ORIENTATION;
	skip_whitespace = (nds_skip_whitespace)FUN_SKIP_WHITESPACE;
	
	printf("[trngaje] sdl_load_config_file\n");
	__sprintf_chk(acStack_428,1,0x420,"%s%cconfig%c%s",param_1 + 0x8a338,0x2f,0x2f,param_2);
	__printf_chk(1,"Loading config file %s\n",acStack_428);
	__stream = fopen(acStack_428,"rb");
	if (__stream == (FILE *)0x0) {
		__printf_chk(1,"Config file %s does not exist.\n",acStack_428);
		uVar5 = 0xffffffff;
	}
	else {
		fread(acStack_838,4,1,__stream);
		fread(auStack_834,4,1,__stream);
		fread(auStack_830,8,1,__stream);
		iVar2 = strncmp(acStack_838,"DSCF",4);
		if (iVar2 == 0) {
			fclose(__stream);
			uVar5 = load_config_file_binary(param_1,param_2,param_3);
			if ((int)uVar5 != -1) {
				__printf_chk(1," Saving converted config file %s.\n",acStack_428);
				save_config_file(param_1,param_2,param_3);
				uVar5 = 0;
			}
		}
		else {
            int i;
			fseek(__stream,0,0);
			while (pcVar3 = fgets(acStack_828,0x400,__stream), pcVar3 != (char *)0x0) {
				pcVar3 = strchr(acStack_828,0x3d);
				if (pcVar3 != (char *)0x0) {
					*pcVar3 = '\0';
					__s1 = (char *)skip_whitespace(acStack_828);
					chomp_whitespace(__s1); // by trngaje
					pcVar3 = (char *)skip_whitespace(pcVar3 + 1);
					chomp_whitespace(pcVar3); // by trngaje
					iVar2 = strcasecmp(__s1,"frameskip_type");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859a8) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"frameskip_value");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859ac) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"safe_frameskip");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x85a10) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"show_frame_counter");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859b0) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"screen_orientation");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859b4) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"screen_scaling");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859b8) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"screen_swap");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859bc) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"savestate_number");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859c0) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"fast_forward");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859c4) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"enable_sound");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859c8) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"clock_speed");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859cc) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"threaded_3d");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859d0) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"mirror_touch");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859d4) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"compress_savestates");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859d8) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"savestate_snapshot");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859dc) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"unzip_roms");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859e4) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"backup_in_savestates");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859e8) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"ignore_gamecard_limit");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859ec) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"frame_interval");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859f0) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"trim_roms");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859f8) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"fix_main_2d_screen");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859fc) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"disable_edge_marking");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x85a00) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"hires_3d");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x85a04) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"use_rtc_custom_time");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x85a1c) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"rtc_custom_time");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(long *)(param_1 + 0x85a20) = lVar6;
					}
					iVar2 = strcasecmp(__s1,"rtc_system_time");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x85a28) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"slot2_device_type");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x85a14) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"rumble_frames");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x85a18) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"firmware.username");
					if (iVar2 == 0) {
						mbstowcs((wchar_t *)(param_1 + 0x85568),pcVar3,10);
						*(unsigned int *)(param_1 + 0x85590) = 0;
					}
					iVar2 = strcasecmp(__s1,"firmware.language");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x85594) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"firmware.favorite_color");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x85598) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"firmware.birthday_month");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x8559c) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"firmware.birthday_day");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						//*(int *)(rtc_load_savestate + param_1) = (int)lVar6; // rtc_load_savestate가 함수 인데.. 32bit : (system->config).firmware.birthday_day = uVar5;
						// w0,[x22, #0x1a0] => 0x855a0
						*(int *)(param_1 + 0x855a0) = (int)lVar6; 
					}
					iVar2 = strcasecmp(__s1,"enable_cheats");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859e0) = (int)lVar6;
					}
                    
					iVar2 = strcasecmp(__s1,"backup_use_sav_format");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advdrastic.uiUseSAVformat = (int)lVar6;
					}

					iVar2 = strcasecmp(__s1,"rotate_dpad");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advdrastic.uiRotateDPAD = (int)lVar6;
					}

					iVar2 = strcasecmp(__s1,"rotate_buttons");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advdrastic.uiRotateButtons = (int)lVar6;
					}

					iVar2 = strcasecmp(__s1,"enable_autosavestate");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advdrastic.uiEnableautosavestate = (int)lVar6;
					}
 
					iVar2 = strcasecmp(__s1,"autosavestate_slot");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advdrastic.uiSlotOfAutosavestate = (int)lVar6;
					}
                   
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UP]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a2c) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_DOWN]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a2e) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_LEFT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a30) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_RIGHT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a32) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_A]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a34) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_B]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a36) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_X]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a38) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_Y]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a3a) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_L]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a3c) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_R]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a3e) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_START]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a40) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_SELECT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a42) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HINGE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a44) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_TOUCH_CURSOR_UP]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a46) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_TOUCH_CURSOR_DOWN]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a48) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_TOUCH_CURSOR_LEFT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a4a) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_TOUCH_CURSOR_RIGHT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a4c) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_TOUCH_CURSOR_PRESS]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a4e) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_MENU]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a50) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_SAVE_STATE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a52) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_LOAD_STATE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a54) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_FAST_FORWARD]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a56) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_SWAP_SCREENS]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a58) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_SWAP_ORIENTATION_A]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a5a) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_SWAP_ORIENTATION_B]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a5c) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_LOAD_GAME]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a5e) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_QUIT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a60) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_FAKE_MICROPHONE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a62) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UI_UP]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a6a) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UI_DOWN]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a6c) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UI_LEFT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a6e) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UI_RIGHT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a70) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UI_SELECT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a72) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UI_BACK]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a76) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UI_EXIT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a74) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UI_PAGE_UP]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a78) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UI_PAGE_DOWN]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a7a) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_UI_SWITCH]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a7c) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HOT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_ASSIGN_HOT].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_VERTICAL_UP]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_UP].a= (short)lVar6;
					}                    
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_VERTICAL_DOWN]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_DOWN].a= (short)lVar6;
					} 
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_VERTICAL_LEFT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_LEFT].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_VERTICAL_RIGHT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_RIGHT].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_CHANGE_LAYOUT_DEC]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_CHANGE_LAYOUT_DEC].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_CHANGE_LAYOUT_INC]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_CHANGE_LAYOUT_INC].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_TOGGLE_DPAD_MOUSE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_TOGGLE_DPAD_MOUSE].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HOT_TOGGLE_DPAD_MOUSE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_DPAD_MOUSE].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HOT_TOGGLE_BLUR_PIXEL]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_BLUR_PIXEL].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HOT_CHANGE_THEME]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_CHANGE_THEME].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HOT_ENTER_MENU]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_ENTER_MENU].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HOT_CUSTOM_SETTING]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_CUSTOM_SETTING].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HOT_TOGGLE_FAST_FORWARD]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_FAST_FORWARD].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HOT_SAVE_STATE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_SAVE_STATE].a= (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HOT_LOAD_STATE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_LOAD_STATE].a= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_a[CONTROL_INDEX_HOT_QUIT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_QUIT].a= (short)lVar6;
					}   
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UP]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a7e) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_DOWN]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a80) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_LEFT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a82) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_RIGHT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a84) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_A]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a86) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_B]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a88) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_X]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a8a) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_Y]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a8c) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_L]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a8e) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_R]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a90) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_START]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a92) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_SELECT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a94) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HINGE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a96) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_TOUCH_CURSOR_UP]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a98) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_TOUCH_CURSOR_DOWN]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a9a) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_TOUCH_CURSOR_LEFT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a9c) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_TOUCH_CURSOR_RIGHT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85a9e) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_TOUCH_CURSOR_PRESS]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85aa0) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_MENU]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85aa2) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_SAVE_STATE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85aa4) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_LOAD_STATE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85aa6) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_FAST_FORWARD]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85aa8) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_SWAP_SCREENS]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85aaa) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_SWAP_ORIENTATION_A]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85aac) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_SWAP_ORIENTATION_B]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85aae) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_LOAD_GAME]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85ab0) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_QUIT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85ab2) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_FAKE_MICROPHONE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85ab4) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UI_UP]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85abc) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UI_DOWN]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85abe) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UI_LEFT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85ac0) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UI_RIGHT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85ac2) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UI_SELECT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85ac4) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UI_BACK]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85ac8) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UI_EXIT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85ac6) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UI_PAGE_UP]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85aca) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UI_PAGE_DOWN]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85acc) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_UI_SWITCH]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(short *)(param_1 + 0x85ace) = (short)lVar6;
					}
					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HOT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_ASSIGN_HOT].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_VERTICAL_UP]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_UP].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_VERTICAL_DOWN]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_DOWN].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_VERTICAL_LEFT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_LEFT].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_VERTICAL_RIGHT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_RIGHT].b= (short)lVar6;
					}
                    
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_CHANGE_LAYOUT_DEC]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_CHANGE_LAYOUT_DEC].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_CHANGE_LAYOUT_INC]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_CHANGE_LAYOUT_INC].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_TOGGLE_DPAD_MOUSE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_TOGGLE_DPAD_MOUSE].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HOT_TOGGLE_DPAD_MOUSE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_DPAD_MOUSE].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HOT_TOGGLE_BLUR_PIXEL]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_BLUR_PIXEL].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HOT_CHANGE_THEME]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_CHANGE_THEME].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HOT_ENTER_MENU]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_ENTER_MENU].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HOT_CUSTOM_SETTING]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_CUSTOM_SETTING].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HOT_TOGGLE_FAST_FORWARD]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_FAST_FORWARD].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HOT_SAVE_STATE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_SAVE_STATE].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HOT_LOAD_STATE]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_LOAD_STATE].b= (short)lVar6;
					}
 					iVar2 = strcasecmp(__s1,"controls_b[CONTROL_INDEX_HOT_QUIT]");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						g_advControls[ADVANCE_CONTROL_INDEX_HOT_QUIT].b= (short)lVar6;
					}        
					iVar2 = strcasecmp(__s1,"batch_threads_3d_count");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x859f4) = (int)lVar6;
					}
					iVar2 = strcasecmp(__s1,"bypass_3d");
					if (iVar2 == 0) {
						lVar6 = strtol(pcVar3,(char **)0x0,10);
						*(int *)(param_1 + 0x85a08) = (int)lVar6;
					}
				}
			}
			set_screen_orientation(*(unsigned int *)(param_1 + 0x859b4));
			__s = (void *)(param_1 + 0x85ad0);
			set_screen_scale_factor(0);
			set_screen_swap(*(unsigned int *)(param_1 + 0x859bc));
			memset(__s,0,0x4000);
			uVar4 = 0;
			do {    // 키 맵핑 하는 부분 인 듯
				uVar1 = *(ushort *)(param_1 + 0x85a2c + uVar4 * 2);
				if (uVar1 != 0xffff) {
					*(ulong *)((long)__s + (ulong)uVar1 * 8) =
						*(ulong *)((long)__s + (ulong)uVar1 * 8) | 1L << (uVar4 & 0x3f);
                    printf("[trngaje] input_a(%d):%p\n", uVar4, *(ulong *)((long)__s + (ulong)uVar1 * 8) );
				}
				uVar1 = *(ushort *)(param_1 + 0x85a7e + uVar4 * 2);
				if (uVar1 != 0xffff) {
					*(ulong *)((long)__s + (ulong)uVar1 * 8) =
						*(ulong *)((long)__s + (ulong)uVar1 * 8) | 1L << (uVar4 & 0x3f);
                    printf("[trngaje] input_b(%d):%p\n", uVar4, *(ulong *)((long)__s + (ulong)uVar1 * 8) );
				}
				uVar4 = uVar4 + 1;
			} while (uVar4 != 0x29);
            
            // 추가 키맵
            for (i=0; i<NUM_OF_ADVANCE_CONTROL_INDEX; i++) {
 				uVar1 = g_advControls[i].a;
				if (uVar1 != 0xffff) {
					*(ulong *)((long)__s + (ulong)uVar1 * 8) =
						*(ulong *)((long)__s + (ulong)uVar1 * 8) | 1L << (uVar4 & 0x3f);
                    //printf("[trngaje] input_a(%d):%p\n", uVar4, *(ulong *)((long)__s + (ulong)uVar1 * 8) );
				}
				uVar1 = g_advControls[i].b;
				if (uVar1 != 0xffff) {
					*(ulong *)((long)__s + (ulong)uVar1 * 8) =
						*(ulong *)((long)__s + (ulong)uVar1 * 8) | 1L << (uVar4 & 0x3f);
                    //printf("[trngaje] input_b(%d):%p\n", uVar4, *(ulong *)((long)__s + (ulong)uVar1 * 8) );
				}
				uVar4 = uVar4 + 1;
            }   
			uVar5 = 0;
		}
	}

	return;
}


void sdl_save_config_file(long param_1, unsigned long param_2, int param_3)
{
    FILE *__stream;
    unsigned long uVar1;
    char acStack_428 [1056];
  
    printf("[trngaje] sdl_save_config_file:param_3=%d\n", param_3);
    __sprintf_chk(acStack_428,1,0x420,"%s%cconfig%c%s",param_1 + 0x8a338,0x2f,0x2f,param_2);

    __stream = fopen(acStack_428,"wt");
    if (__stream == (FILE *)0x0) {
        __printf_chk(1,"ERROR: Couldn\'t save config file %s.\n",acStack_428);
        uVar1 = 0xffffffff;
    }
    else {
        __printf_chk(1,"Saving config to %s\n",acStack_428);
        __fprintf_chk(__stream,1,"%s = %d\n","frameskip_type",*(unsigned int *)(param_1 + 0x859a8));
        __fprintf_chk(__stream,1,"%s = %d\n","frameskip_value",*(unsigned int *)(param_1 + 0x859ac));
        __fprintf_chk(__stream,1,"%s = %d\n","safe_frameskip",*(unsigned int *)(param_1 + 0x85a10));
        __fprintf_chk(__stream,1,"%s = %d\n","show_frame_counter",*(unsigned int *)(param_1 + 0x859b0));
        __fprintf_chk(__stream,1,"%s = %d\n","screen_orientation",*(unsigned int *)(param_1 + 0x859b4));
        __fprintf_chk(__stream,1,"%s = %d\n","screen_scaling",*(unsigned int *)(param_1 + 0x859b8));
        __fprintf_chk(__stream,1,"%s = %d\n","screen_swap",*(unsigned int *)(param_1 + 0x859bc));
        __fprintf_chk(__stream,1,"%s = %d\n","savestate_number",*(unsigned int *)(param_1 + 0x859c0));
        __fprintf_chk(__stream,1,"%s = %d\n","fast_forward",*(unsigned int *)(param_1 + 0x859c4));
        __fprintf_chk(__stream,1,"%s = %d\n","enable_sound",*(unsigned int *)(param_1 + 0x859c8));
        __fprintf_chk(__stream,1,"%s = %d\n","clock_speed",*(unsigned int *)(param_1 + 0x859cc));
        __fprintf_chk(__stream,1,"%s = %d\n","threaded_3d",*(unsigned int *)(param_1 + 0x859d0));
        __fprintf_chk(__stream,1,"%s = %d\n","mirror_touch",*(unsigned int *)(param_1 + 0x859d4));
        __fprintf_chk(__stream,1,"%s = %d\n","compress_savestates",*(unsigned int *)(param_1 + 0x859d8));
        __fprintf_chk(__stream,1,"%s = %d\n","savestate_snapshot",*(unsigned int *)(param_1 + 0x859dc));
        __fprintf_chk(__stream,1,"%s = %d\n","unzip_roms",*(unsigned int *)(param_1 + 0x859e4));
        __fprintf_chk(__stream,1,"%s = %d\n","backup_in_savestates",*(unsigned int *)(param_1 + 0x859e8));
        __fprintf_chk(__stream,1,"%s = %d\n","ignore_gamecard_limit",*(unsigned int *)(param_1 + 0x859ec));
        __fprintf_chk(__stream,1,"%s = %d\n","frame_interval",*(unsigned int *)(param_1 + 0x859f0));
        __fprintf_chk(__stream,1,"%s = %d\n","trim_roms",*(unsigned int *)(param_1 + 0x859f8));
        __fprintf_chk(__stream,1,"%s = %d\n","fix_main_2d_screen",*(unsigned int *)(param_1 + 0x859fc));
        __fprintf_chk(__stream,1,"%s = %d\n","disable_edge_marking",*(unsigned int *)(param_1 + 0x85a00));
        __fprintf_chk(__stream,1,"%s = %d\n","hires_3d",*(unsigned int *)(param_1 + 0x85a04));
        __fprintf_chk(__stream,1,"%s = %d\n","use_rtc_custom_time",*(unsigned int *)(param_1 + 0x85a1c));
        __fprintf_chk(__stream,1,"%s = %d\n","rtc_custom_time",*(unsigned int *)(param_1 + 0x85a20));
        __fprintf_chk(__stream,1,"%s = %d\n","rtc_system_time",*(unsigned int *)(param_1 + 0x85a28));
        __fprintf_chk(__stream,1,"%s = %d\n","slot2_device_type",*(unsigned int *)(param_1 + 0x85a14));
        __fprintf_chk(__stream,1,"%s = %d\n","rumble_frames",*(unsigned int *)(param_1 + 0x85a18));
        __fprintf_chk(__stream,1,"%s = %ls\n","firmware.username",param_1 + 0x85568);
        __fprintf_chk(__stream,1,"%s = %d\n","firmware.language",*(unsigned int *)(param_1 + 0x85594));
        __fprintf_chk(__stream,1,"%s = %d\n","firmware.favorite_color",  *(unsigned int *)(param_1 + 0x85598));
        __fprintf_chk(__stream,1,"%s = %d\n","firmware.birthday_month",   *(unsigned int *)(param_1 + 0x8559c));
       // __fprintf_chk(__stream,1,"%s = %d\n","firmware.birthday_day",   *(unsigned int *)(rtc_load_savestate + param_1));
        __fprintf_chk(__stream,1,"%s = %d\n","firmware.birthday_day",   *(unsigned int *)(param_1 + 0x855a0));
    
        if (param_3 == 0) {
            __fprintf_chk(__stream,1,"%s = %d\n","enable_cheats",*(unsigned int *)(param_1 + 0x859e0));
        }
        
        __fprintf_chk(__stream,1,"%s = %d\n","backup_use_sav_format",   g_advdrastic.uiUseSAVformat);
        
        __fprintf_chk(__stream,1,"%s = %d\n","rotate_dpad",   g_advdrastic.uiRotateDPAD);

        __fprintf_chk(__stream,1,"%s = %d\n","rotate_buttons",   g_advdrastic.uiRotateButtons);

        __fprintf_chk(__stream,1,"%s = %d\n","enable_autosavestate",   g_advdrastic.uiEnableautosavestate);

        __fprintf_chk(__stream,1,"%s = %d\n","autosavestate_slot",   g_advdrastic.uiSlotOfAutosavestate);

        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UP]",
                      *(unsigned short *)(param_1 + 0x85a2c));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_DOWN]",
                      *(unsigned short *)(param_1 + 0x85a2e));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_LEFT]",
                      *(unsigned short *)(param_1 + 0x85a30));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_RIGHT]",
                      *(unsigned short *)(param_1 + 0x85a32));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_A]",
                      *(unsigned short *)(param_1 + 0x85a34));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_B]",
                      *(unsigned short *)(param_1 + 0x85a36));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_X]",
                      *(unsigned short *)(param_1 + 0x85a38));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_Y]",
                      *(unsigned short *)(param_1 + 0x85a3a));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_L]",
                      *(unsigned short *)(param_1 + 0x85a3c));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_R]",
                      *(unsigned short *)(param_1 + 0x85a3e));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_START]",
                      *(unsigned short *)(param_1 + 0x85a40));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_SELECT]",
                      *(unsigned short *)(param_1 + 0x85a42));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HINGE]",
                      *(unsigned short *)(param_1 + 0x85a44));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_TOUCH_CURSOR_UP]",
                      *(unsigned short *)(param_1 + 0x85a46));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_TOUCH_CURSOR_DOWN]",
                      *(unsigned short *)(param_1 + 0x85a48));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_TOUCH_CURSOR_LEFT]",
                      *(unsigned short *)(param_1 + 0x85a4a));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_TOUCH_CURSOR_RIGHT]",
                      *(unsigned short *)(param_1 + 0x85a4c));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_TOUCH_CURSOR_PRESS]",
                      *(unsigned short *)(param_1 + 0x85a4e));
        if (param_3 == 0) {
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_MENU]",
                        *(unsigned short *)(param_1 + 0x85a50));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_SAVE_STATE]",
                        *(unsigned short *)(param_1 + 0x85a52));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_LOAD_STATE]",
                        *(unsigned short *)(param_1 + 0x85a54));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_FAST_FORWARD]",
                        *(unsigned short *)(param_1 + 0x85a56));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_SWAP_SCREENS]",
                        *(unsigned short *)(param_1 + 0x85a58));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_SWAP_ORIENTATION_A]",
                        *(unsigned short *)(param_1 + 0x85a5a));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_SWAP_ORIENTATION_B]",
                        *(unsigned short *)(param_1 + 0x85a5c));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_LOAD_GAME]",
                        *(unsigned short *)(param_1 + 0x85a5e));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_QUIT]",
                        *(unsigned short *)(param_1 + 0x85a60));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_FAKE_MICROPHONE]",
                        *(unsigned short *)(param_1 + 0x85a62));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UI_UP]",
                        *(unsigned short *)(param_1 + 0x85a6a));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UI_DOWN]",
                        *(unsigned short *)(param_1 + 0x85a6c));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UI_LEFT]",
                        *(unsigned short *)(param_1 + 0x85a6e));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UI_RIGHT]",
                        *(unsigned short *)(param_1 + 0x85a70));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UI_SELECT]",
                        *(unsigned short *)(param_1 + 0x85a72));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UI_BACK]",
                        *(unsigned short *)(param_1 + 0x85a76));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UI_EXIT]",
                        *(unsigned short *)(param_1 + 0x85a74));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UI_PAGE_UP]",
                        *(unsigned short *)(param_1 + 0x85a78));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UI_PAGE_DOWN]",
                        *(unsigned short *)(param_1 + 0x85a7a));
            __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_UI_SWITCH]",
                        *(unsigned short *)(param_1 + 0x85a7c));
        }
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HOT]", g_advControls[ADVANCE_CONTROL_INDEX_ASSIGN_HOT].a);
        
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_VERTICAL_UP]", g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_UP].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_VERTICAL_DOWN]", g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_DOWN].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_VERTICAL_LEFT]", g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_LEFT].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_VERTICAL_RIGHT]", g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_RIGHT].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_CHANGE_LAYOUT_DEC]", g_advControls[ADVANCE_CONTROL_INDEX_CHANGE_LAYOUT_DEC].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_CHANGE_LAYOUT_INC]", g_advControls[ADVANCE_CONTROL_INDEX_CHANGE_LAYOUT_INC].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_TOGGLE_DPAD_MOUSE]", g_advControls[ADVANCE_CONTROL_INDEX_TOGGLE_DPAD_MOUSE].a);
        
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HOT_TOGGLE_DPAD_MOUSE]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_DPAD_MOUSE].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HOT_TOGGLE_BLUR_PIXEL]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_BLUR_PIXEL].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HOT_CHANGE_THEME]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_CHANGE_THEME].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HOT_ENTER_MENU]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_ENTER_MENU].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HOT_CUSTOM_SETTING]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_CUSTOM_SETTING].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HOT_TOGGLE_FAST_FORWARD]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_FAST_FORWARD].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HOT_SAVE_STATE]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_SAVE_STATE].a);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HOT_LOAD_STATE]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_LOAD_STATE].a);
        
        __fprintf_chk(__stream,1,"%s = %d\n","controls_a[CONTROL_INDEX_HOT_QUIT]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_QUIT].a);
    
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UP]",
                      *(unsigned short *)(param_1 + 0x85a7e));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_DOWN]",
                      *(unsigned short *)(param_1 + 0x85a80));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_LEFT]",
                      *(unsigned short *)(param_1 + 0x85a82));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_RIGHT]",
                      *(unsigned short *)(param_1 + 0x85a84));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_A]",
                      *(unsigned short *)(param_1 + 0x85a86));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_B]",
                      *(unsigned short *)(param_1 + 0x85a88));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_X]",
                      *(unsigned short *)(param_1 + 0x85a8a));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_Y]",
                      *(unsigned short *)(param_1 + 0x85a8c));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_L]",
                      *(unsigned short *)(param_1 + 0x85a8e));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_R]",
                      *(unsigned short *)(param_1 + 0x85a90));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_START]",
                      *(unsigned short *)(param_1 + 0x85a92));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_SELECT]",
                      *(unsigned short *)(param_1 + 0x85a94));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HINGE]",
                      *(unsigned short *)(param_1 + 0x85a96));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_TOUCH_CURSOR_UP]",
                      *(unsigned short *)(param_1 + 0x85a98));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_TOUCH_CURSOR_DOWN]",
                      *(unsigned short *)(param_1 + 0x85a9a));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_TOUCH_CURSOR_LEFT]",
                      *(unsigned short *)(param_1 + 0x85a9c));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_TOUCH_CURSOR_RIGHT]",
                      *(unsigned short *)(param_1 + 0x85a9e));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_TOUCH_CURSOR_PRESS]",
                      *(unsigned short *)(param_1 + 0x85aa0));
    if (param_3 == 0) {
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_MENU]",
                    *(unsigned short *)(param_1 + 0x85aa2));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_SAVE_STATE]",
                    *(unsigned short *)(param_1 + 0x85aa4));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_LOAD_STATE]",
                    *(unsigned short *)(param_1 + 0x85aa6));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_FAST_FORWARD]",
                    *(unsigned short *)(param_1 + 0x85aa8));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_SWAP_SCREENS]",
                    *(unsigned short *)(param_1 + 0x85aaa));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_SWAP_ORIENTATION_A]",
                    *(unsigned short *)(param_1 + 0x85aac));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_SWAP_ORIENTATION_B]",
                    *(unsigned short *)(param_1 + 0x85aae));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_LOAD_GAME]",
                    *(unsigned short *)(param_1 + 0x85ab0));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_QUIT]",
                    *(unsigned short *)(param_1 + 0x85ab2));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_FAKE_MICROPHONE]",
                    *(unsigned short *)(param_1 + 0x85ab4));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UI_UP]",
                    *(unsigned short *)(param_1 + 0x85abc));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UI_DOWN]",
                    *(unsigned short *)(param_1 + 0x85abe));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UI_LEFT]",
                    *(unsigned short *)(param_1 + 0x85ac0));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UI_RIGHT]",
                    *(unsigned short *)(param_1 + 0x85ac2));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UI_SELECT]",
                    *(unsigned short *)(param_1 + 0x85ac4));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UI_BACK]",
                    *(unsigned short *)(param_1 + 0x85ac8));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UI_EXIT]",
                    *(unsigned short *)(param_1 + 0x85ac6));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UI_PAGE_UP]",
                    *(unsigned short *)(param_1 + 0x85aca));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UI_PAGE_DOWN]",
                    *(unsigned short *)(param_1 + 0x85acc));
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_UI_SWITCH]",
                    *(unsigned short *)(param_1 + 0x85ace));
        }
        
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HOT]", g_advControls[ADVANCE_CONTROL_INDEX_ASSIGN_HOT].b);
        
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_VERTICAL_UP]", g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_UP].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_VERTICAL_DOWN]", g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_DOWN].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_VERTICAL_LEFT]", g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_LEFT].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_VERTICAL_RIGHT]", g_advControls[ADVANCE_CONTROL_INDEX_VERTICAL_RIGHT].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_CHANGE_LAYOUT_DEC]", g_advControls[ADVANCE_CONTROL_INDEX_CHANGE_LAYOUT_DEC].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_CHANGE_LAYOUT_INC]", g_advControls[ADVANCE_CONTROL_INDEX_CHANGE_LAYOUT_INC].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_TOGGLE_DPAD_MOUSE]", g_advControls[ADVANCE_CONTROL_INDEX_TOGGLE_DPAD_MOUSE].b);
        
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HOT_TOGGLE_DPAD_MOUSE]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_DPAD_MOUSE].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HOT_TOGGLE_BLUR_PIXEL]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_BLUR_PIXEL].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HOT_CHANGE_THEME]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_CHANGE_THEME].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HOT_ENTER_MENU]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_ENTER_MENU].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HOT_CUSTOM_SETTING]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_CUSTOM_SETTING].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HOT_TOGGLE_FAST_FORWARD]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_TOGGLE_FAST_FORWARD].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HOT_SAVE_STATE]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_SAVE_STATE].b);
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HOT_LOAD_STATE]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_LOAD_STATE].b);
        
        __fprintf_chk(__stream,1,"%s = %d\n","controls_b[CONTROL_INDEX_HOT_QUIT]", g_advControls[ADVANCE_CONTROL_INDEX_HOT_QUIT].b);
        
        fclose(__stream);
        uVar1 = 0;
    }

}

void sdl_config_update_settings(long param_1)
{
    void *__s;
    ushort uVar1;
    ulong uVar2;
    ulong uVar3;
    int i;
    
    nds_set_screen_orientation      set_screen_orientation;
    nds_set_screen_scale_factor    set_screen_scale_factor;
    nds_set_screen_swap                  set_screen_swap;
    
    set_screen_orientation = (nds_set_screen_orientation)FUN_SET_SCREEN_ORIENTATION;
    set_screen_scale_factor = (nds_set_screen_scale_factor)FUN_SET_SCREEN_SCALE_FACTOR;
    set_screen_swap = (nds_set_screen_swap)FUN_SET_SCREEN_SWAP;
    
    __s = (void *)(param_1 + 0x568);
    set_screen_orientation(*(unsigned int *)(param_1 + 0x44c));
    set_screen_scale_factor(0);
    set_screen_swap(*(unsigned int *)(param_1 + 0x454));
    memset(__s,0,0x4000);
    uVar3 = 0;
    
    printf("[trngaje] sdl_config_update_settings\n");
    do {
        uVar1 = *(ushort *)(param_1 + 0x4c4 + uVar3 * 2);
        uVar2 = uVar3 & 0x3f;
        if (uVar1 != 0xffff) {
            *(ulong *)((long)__s + (ulong)uVar1 * 8) =
                *(ulong *)((long)__s + (ulong)uVar1 * 8) | 1L << (uVar3 & 0x3f);
        }
        uVar1 = *(ushort *)(param_1 + 0x516 + uVar3 * 2);
        uVar3 = uVar3 + 1;
        if (uVar1 != 0xffff) {
            *(ulong *)((long)__s + (ulong)uVar1 * 8) =
                *(ulong *)((long)__s + (ulong)uVar1 * 8) | 1L << uVar2;
        }
    } while (uVar3 != 0x29);
    
    // 추가한 키 맵핑
    for (i=0; i<NUM_OF_ADVANCE_CONTROL_INDEX; i++) {
        uVar1 = g_advControls[i].a;

        if (uVar1 != 0xffff) {
            *(ulong *)((long)__s + (ulong)uVar1 * 8) =
                *(ulong *)((long)__s + (ulong)uVar1 * 8) | 1L << (uVar3 & 0x3f);
        }
        uVar1 = g_advControls[i].b;

        if (uVar1 != 0xffff) {
            *(ulong *)((long)__s + (ulong)uVar1 * 8) =
                *(ulong *)((long)__s + (ulong)uVar1 * 8) | 1L << (uVar3 & 0x3f);
        }
        uVar3 = uVar3 + 1;
    }
    
    return;
}

unsigned char  ** sdl_create_menu_main(long param_1)
{
	unsigned char **ppcVar1;
	unsigned char *pcVar2;
	char *pcVar3;
	char *pcVar4;
	char *pcVar5;
	char **ppcVar6;
	char **ppcVar7;
	char **ppcVar8;
	unsigned int uVar9;
	unsigned long *puVar10;
	long lVar11;
 
    nds_create_menu_options create_menu_options;
    nds_create_menu_controls create_menu_controls;
    nds_create_menu_firmware create_menu_firmware;
    nds_draw_menu_option draw_menu_option;
    nds_action_select_menu action_select_menu;
    nds_destroy_select_menu destroy_select_menu;
    nds_select_cheat_menu select_cheat_menu;
    nds_focus_savestate focus_savestate;
    nds_modify_snapshot_bg modify_snapshot_bg;
    nds_select_load_state select_load_state;
    nds_draw_numeric draw_numeric;
    nds_action_numeric action_numeric;
    nds_action_numeric_select action_numeric_select;
    nds_select_save_state select_save_state;
    nds_action_select action_select;
    nds_select_load_game select_load_game;
    nds_select_restart select_restart;
    nds_select_return select_return;
    nds_select_quit select_quit;
    nds_draw_menu_main draw_menu_main;
    
    create_menu_options = (nds_create_menu_options)FUN_CREATE_MENU_OPTION;
    create_menu_controls = (nds_create_menu_controls)FUN_CREATE_MENU_COLTROLS;
    create_menu_firmware = (nds_create_menu_firmware)FUN_CREATE_MENU_FIRMWARE;
    draw_menu_option = (nds_draw_menu_option)FUN_DRAW_MENU_OPTION;
    action_select_menu = (nds_action_select_menu)FUN_ACTION_SELECT_MENU;
    destroy_select_menu = (nds_destroy_select_menu)FUN_DESTROY_SELECT_MENU;
    select_cheat_menu = (nds_select_cheat_menu)FUN_SELECT_CHEAT_MENU;
    focus_savestate = (nds_focus_savestate)FUN_FOCUS_SAVESTATE;
    modify_snapshot_bg = (nds_modify_snapshot_bg)FUN_MODIFY_SNAPSHOT_BG;
    select_load_state = (nds_select_load_state)FUN_SELECT_LOAD_STATE;
    draw_numeric = (nds_draw_numeric)FUN_DRAW_NUMERIC;
    action_numeric = (nds_action_numeric)FUN_ACTION_NUMERIC;
    action_numeric_select = (nds_action_numeric_select)FUN_ACTION_NUMERIC_SELECT;
    select_save_state = (nds_select_save_state)FUN_SELECT_SAVE_STATE;
    action_select = (nds_action_select)FUN_ACTION_SELECT;
    select_load_game = (nds_select_load_game)FUN_SELECT_LOAD_GAME;
    select_restart = (nds_select_restart)FUN_SELECT_RESTART;
    select_return = (nds_select_return)FUN_SELECT_RETURN;
    select_quit = (nds_select_quit)FUN_SELECT_QUIT;
    draw_menu_main = (nds_draw_menu_main)FUN_DRAW_MENU_MAIN;
    
    printf("[trngaje] sdl_create_menu_main++\n");
    
	lVar11 = *(long *)(param_1 + 8);
	ppcVar1 = (unsigned char **)malloc(0x30);
	*(unsigned long *)((long)ppcVar1 + 0x14) = 10+1;
	*ppcVar1 = draw_menu_main;
	ppcVar1[1] = (unsigned char *)0x0;
	ppcVar1[5] = (unsigned char *)0x0;
	pcVar2 = (unsigned char *)malloc(0x50+8);
	ppcVar1[4] = pcVar2;
	pcVar3 = (char *)create_menu_options(param_1,ppcVar1);
	pcVar4 = (char *)create_menu_controls(param_1,ppcVar1);
	pcVar5 = (char *)create_menu_firmware(param_1,ppcVar1);
	puVar10 = (unsigned long *)ppcVar1[4];
	uVar9 = 0x158;
	if (*(int *)(param_1 + 0x40) != 0) {
		uVar9 = 0xb4;
	}
	*(unsigned int *)(ppcVar1 + 2) = uVar9;
    // ----------------------------------------------------------------
	ppcVar6 = (char **)malloc(0x38);
	ppcVar8 = ppcVar6;
	if (ppcVar6 == (char **)0x0) {
		ppcVar8 = (char **)malloc(0x30);
	}
	*ppcVar8 = "Change Options";
	*(unsigned int *)(ppcVar8 + 1) = 0x23; // 45
	ppcVar8[2] = (char *)draw_menu_option;
	ppcVar8[3] = (char *)action_select_menu;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)destroy_select_menu;
	ppcVar6[6] = pcVar3;
	*puVar10 = ppcVar6;
    // -----------------------------------------------------------------------
    // add Change Steward Options
 	ppcVar6 = (char **)malloc(0x38);
	ppcVar8 = ppcVar6;
	if (ppcVar6 == (char **)0x0) {
		ppcVar8 = (char **)malloc(0x30);
	}
	*ppcVar8 = "Change Steward Options";
	*(unsigned int *)(ppcVar8 + 1) = 0x23+1; // 45
	ppcVar8[2] = (char *)draw_menu_option;
	ppcVar8[3] = (char *)action_select;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)0x0;
	ppcVar6[6] = display_custom_setting; 
	puVar10[1] = ppcVar6;
    // -----------------------------------------------------------------------
	ppcVar6 = (char **)malloc(0x38);
	ppcVar8 = ppcVar6;
	if (ppcVar6 == (char **)0x0) {
		ppcVar8 = (char **)malloc(0x30);
	}
	*(unsigned int *)(ppcVar8 + 1) = 0x24+1;
	*ppcVar8 = "Configure Controls";
	ppcVar8[2] = (char *)draw_menu_option;
	ppcVar8[3] = (char *)action_select_menu;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)destroy_select_menu;
	ppcVar6[6] = pcVar4;
	puVar10[1+1] = ppcVar6;
	ppcVar6 = (char **)malloc(0x38);
	ppcVar8 = ppcVar6;
	if (ppcVar6 == (char **)0x0) {
		ppcVar8 = (char **)malloc(0x30);
	}
	*(unsigned int *)(ppcVar8 + 1) = 0x25+1;
	*ppcVar8 = "Configure Firmware";
	ppcVar8[2] = (char *)draw_menu_option;
	ppcVar8[3] = (char *)action_select_menu;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)destroy_select_menu;
	ppcVar6[6] = pcVar5;
	puVar10[2+1] = ppcVar6;
	ppcVar6 = (char **)malloc(0x38);
	ppcVar8 = ppcVar6;
	if (ppcVar6 == (char **)0x0) {
		ppcVar8 = (char **)malloc(0x30);
	}
	*ppcVar8 = "Configure Cheats";
	pcVar3 = (char *)(lVar11 + 0x458);
	*(unsigned int *)(ppcVar8 + 1) = 0x26+1;
	ppcVar8[2] = (char *)draw_menu_option;
	ppcVar8[3] = (char *)action_select;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)0x0;
	ppcVar6[6] = (char *)select_cheat_menu;
	puVar10[3+1] = ppcVar6;
	ppcVar7 = (char **)malloc(0x50);
	ppcVar8 = ppcVar7;
	ppcVar6 = ppcVar7;
	if ((ppcVar7 == (char **)0x0) &&
		(ppcVar6 = (char **)malloc(0x40), ppcVar8 = ppcVar6, ppcVar6 == (char **)0x0)) {
		ppcVar8 = (char **)malloc(0x30);
	}
	*ppcVar8 = "Load state   ";
	*(unsigned int *)(ppcVar8 + 1) = 0x28;
	ppcVar8[2] = (char *)draw_numeric;
	ppcVar8[3] = (char *)action_numeric;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)0x0;
	ppcVar6[6] = pcVar3;
	ppcVar6[7] = (char *)0x900000000;
	ppcVar7[3] = (char *)action_numeric_select;
	ppcVar7[4] = (char *)focus_savestate;
	ppcVar7[8] = (char *)modify_snapshot_bg;
	ppcVar7[9] = (char *)select_load_state;
	puVar10[4+1] = ppcVar7;
	ppcVar7 = (char **)malloc(0x50);
	ppcVar8 = ppcVar7;
	ppcVar6 = ppcVar7;
	if ((ppcVar7 == (char **)0x0) &&
		(ppcVar6 = (char **)malloc(0x40), ppcVar8 = ppcVar6, ppcVar6 == (char **)0x0)) {
		ppcVar8 = (char **)malloc(0x30);
	}
	*ppcVar8 = "Save state   ";
	*(unsigned int *)(ppcVar8 + 1) = 0x29;
	ppcVar8[2] = (char *)draw_numeric;
	ppcVar8[3] = (char *)action_numeric;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)0x0;
	ppcVar6[6] = pcVar3;
	ppcVar6[7] = (char *)0x900000000;
	ppcVar7[3] = (char *)action_numeric_select;
	ppcVar7[4] = (char *)focus_savestate;
	ppcVar7[8] = (char *)modify_snapshot_bg;
	ppcVar7[9] = (char *)select_save_state;
	puVar10[5+1] = ppcVar7;
	ppcVar6 = (char **)malloc(0x38);
	ppcVar8 = ppcVar6;
	if (ppcVar6 == (char **)0x0) {
		ppcVar8 = (char **)malloc(0x30);
	}
	*ppcVar8 = "Load new game";
	*(unsigned int *)(ppcVar8 + 1) = 0x2b;
	ppcVar8[2] = (char *)draw_menu_option;
	ppcVar8[3] = (char *)action_select;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)0x0;
	ppcVar6[6] = (char *)select_load_game;
	puVar10[6+1] = ppcVar6;
	ppcVar6 = (char **)malloc(0x38);
	ppcVar8 = ppcVar6;
	if (ppcVar6 == (char **)0x0) {
		ppcVar8 = (char **)malloc(0x30);
	}
	*ppcVar8 = "Restart game";
	*(unsigned int *)(ppcVar8 + 1) = 0x2c;
	ppcVar8[2] = (char *)draw_menu_option;
	ppcVar8[3] = (char *)action_select;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)0x0;
	ppcVar6[6] = (char *)select_restart;
	puVar10[7+1] = ppcVar6;
	ppcVar6 = (char **)malloc(0x38);
	ppcVar8 = ppcVar6;
	if (ppcVar6 == (char **)0x0) {
		ppcVar8 = (char **)malloc(0x30);
	}
	*ppcVar8 = "Return to game";
	*(unsigned int *)(ppcVar8 + 1) = 0x2e;
	ppcVar8[2] = (char *)draw_menu_option;
	ppcVar8[3] = (char *)action_select;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)0x0;
	ppcVar6[6] = (char *)select_return;
	puVar10[8+1] = ppcVar6;
	ppcVar6 = (char **)malloc(0x38);
	ppcVar8 = ppcVar6;
	if (ppcVar6 == (char **)0x0) {
		ppcVar8 = (char **)malloc(0x30);
	}
	//*ppcVar8 = "Exit DraStic";
    *ppcVar8 = "Exit DraStic-trngaje";
	*(unsigned int *)(ppcVar8 + 1) = 0x30;
	ppcVar8[2] = (char *)draw_menu_option;
	ppcVar8[3] = (char *)action_select;
	ppcVar8[4] = (char *)0x0;
	ppcVar8[5] = (char *)0x0;
	ppcVar6[6] = (char *)select_quit;
	puVar10[9+1] = ppcVar6;
	return ppcVar1;
}

const char *static_text_degrees[4]= {"            0", "           90", "          180", "          270"}; // space 필요함
const char *static_text_slots[11]= {"            0",  "           1",  "          2",  "          3",  "          4", 
    "            5",  "           6",  "          7",  "          8",  "          9", 
    "            10"
}; // space 필요함

unsigned char ** sdl_create_menu_options(long param_1,unsigned char *param_2)
{
    int iVar1;
    unsigned int uVar2;
    unsigned char **ppcVar3;
    unsigned long *puVar4;
    char **ppcVar5;
    char **ppcVar6;
    char **ppcVar7;
    long lVar8;
  
    nds_draw_menu_option    draw_menu_option;
    nds_focus_menu_none focus_menu_none;
    nds_draw_numeric draw_numeric;
    nds_action_numeric action_numeric;
    nds_select_delete_config_local  select_delete_config_local;
    nds_select_save_config_global   select_save_config_global;
    nds_select_exit_current_menu    select_exit_current_menu;
    nds_action_select   action_select;
    nds_draw_numeric_labeled        draw_numeric_labeled;
    nds_select_save_config_local    select_save_config_local;
    nds_draw_menu_options               draw_menu_options;
    
    draw_menu_option = (nds_draw_menu_option)FUN_DRAW_MENU_OPTION;
    focus_menu_none = (nds_focus_menu_none)FUN_FOCUS_MENU_NONE;
    draw_numeric = (nds_draw_numeric)FUN_DRAW_NUMERIC;
    action_numeric = (nds_action_numeric)FUN_ACTION_NUMERIC;
    select_delete_config_local = (nds_select_delete_config_local)FUN_SELECT_DELETE_CONFIG_LOCAL;
    select_save_config_global = (nds_select_save_config_global)FUN_SELECT_SAVE_CONFIG_GLOBAL;
    select_exit_current_menu = (nds_select_exit_current_menu)FUN_SELECT_EXIT_CURRENT_MENU;
    action_select = (nds_action_select)FUN_ACTION_SELECT;   
    draw_numeric_labeled = (nds_draw_numeric_labeled)FUN_DRAW_NUMERIC_LABELED;
    select_save_config_local = (nds_select_save_config_local)FUN_SELECT_SAVE_CONFIG_LOCAL;
    draw_menu_options = (nds_draw_menu_options)FUN_DRAW_MENU_OPTIONS;
    
    lVar8 = *(long *)(param_1 + 8);
    ppcVar3 = (unsigned char **)malloc(0x30);
    *(unsigned long *)((long)ppcVar3 + 0x14) = NUM_OF_MENU_OPTIONS; //0x19+2; //+1;
    *ppcVar3 = draw_menu_options;
    ppcVar3[1] = focus_menu_none;
    ppcVar3[5] = param_2;
    puVar4 = (unsigned long *)malloc(NUM_OF_MENU_OPTIONS * 8/*200+8*3*/);
    ppcVar3[4] = (unsigned char *)puVar4;
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Frame skip type        ";
    *(unsigned int *)(ppcVar6 + 1) = 0x1e;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x440);
    ppcVar7[7] = (char *)0x200000000; // none/manual/automatic
    //*puVar4 = ppcVar5; // index
    puVar4 [DRASTIC_MENU_OPTIONS_FRAME_SKIP_TYPE]= ppcVar5; 
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b410; //frameskip_labels.11546;
    
    ppcVar5 = (char **)malloc(0x40);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x30);
    }
    *(unsigned int  *)(ppcVar7 + 1) = 0x1f;
    *ppcVar7 = "Frame skip value                   ";
    ppcVar7[2] = (char *)draw_numeric;
    ppcVar7[3] = (char *)action_numeric;
    ppcVar7[4] = (char *)0x0;
    ppcVar7[5] = (char *)0x0;
    ppcVar5[6] = (char *)(lVar8 + 0x444);
    ppcVar5[7] = (char *)0x900000001; // 1,2,3,4,5,6,7,8,9
    puVar4[DRASTIC_MENU_OPTIONS_FRAME_SKIP_VALUE] = ppcVar5;
    
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Safe frame skipping    ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x20;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x4a8);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_SAFE_FRAME_SKIPPING] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;// yes_no_labels.11545;
    
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Screen orientation(N/A)";
    *(unsigned int  *)(ppcVar6 + 1) = 0x22;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(param_1 + 0x50);
    ppcVar7[7] = (char *)0x200000000; // vertical/horizontal/single
    puVar4[DRASTIC_MENU_OPTIONS_SCREEN_ORIENTATION] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b440;//orientation_labels.11547;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Screen swap            ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x23;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x454);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_SCREEN_SWAP] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Show speed             ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x24;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x448);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_SHOW_SPEED] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Enable sound           ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x25;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x460);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_ENABLE_SOUND] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Fast forward           ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x26;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x45c);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_FAST_FORWARD] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Mirror touchscreen     ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x27;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x46c);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_MIRROR_TOUCHSCREEN] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Compress savestates    ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x28;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x470);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_COMPRESS_SAVESTATES] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Snapshot in savestates ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x29;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x474);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_SNAPSHOT_IN_SAVESTATES] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Enable cheats          ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x2a;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x478);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_ENABLE_CHEATS] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Uncompress ROMs        ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x2b;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x47c);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_UNCOMPRESS_ROMS] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Backup in savestates   ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x2c;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x480);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_BACKUP_IN_SAVESTATES] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Speed override         ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x2d;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(param_1 + 0x54);
    ppcVar7[7] = (char *)0x700000000;// none, 50%,..400%
    puVar4[DRASTIC_MENU_OPTIONS_SPEED_OVERRIDE] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b460;//speed_override_labels.11550;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Fix main 2D screen     ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x2e;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x494);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_FIX_MAIN_2D_SCREEN] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Disable edge marking   ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x2f;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x498);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_DISABLE_EDGE_MARKING] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Slot 2 Device          ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x30;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x4ac);
    ppcVar7[7] = (char *)0x300000000;//none, gba cart, sram cart,rumble pack
    puVar4[DRASTIC_MENU_OPTIONS_SLOT_2_DEVICE] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b4a0; //slot2_device_type_labels.11551;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "High-resolution 3D     ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x31;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x49c);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_HIGH_RESOLUTION_3D] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Threaded 3D            ";
    *(unsigned int  *)(ppcVar6 + 1) = 0x32;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)(lVar8 + 0x468);
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_THREADED_3D] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;

    // add by trngaje
    ////////////////
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Use raw .sav format (no footer)";
    *(unsigned int  *)(ppcVar6 + 1) = 0x33;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)&g_advdrastic.uiUseSAVformat; 
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_USE_RAW_SAV_FORMAT] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
    
//
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Rotate DPAD";
    *(unsigned int  *)(ppcVar6 + 1) = 0x33+1;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)&g_advdrastic.uiRotateDPAD; 
    ppcVar7[7] = (char *)0x300000000; // max 값 3
    puVar4[DRASTIC_MENU_OPTIONS_ROATE_DPAD] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = static_text_degrees; 
//
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Rotate Buttons";
    *(unsigned int  *)(ppcVar6 + 1) = 0x33+2;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)&g_advdrastic.uiRotateButtons; 
    ppcVar7[7] = (char *)0x300000000; // max 값 3
    puVar4[DRASTIC_MENU_OPTIONS_ROATE_BUTTONS] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = static_text_degrees;
//
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    ppcVar6 = ppcVar7;
    if (ppcVar7 == (char **)0x0) {
        ppcVar6 = (char **)malloc(0x30);
    }
    *ppcVar6 = "Enable auto savestate";
    *(unsigned int  *)(ppcVar6 + 1) = 0x33+2;
    ppcVar6[2] = (char *)draw_numeric;
    ppcVar6[3] = (char *)action_numeric;
    ppcVar6[4] = (char *)0x0;
    ppcVar6[5] = (char *)0x0;
    ppcVar7[6] = (char *)&g_advdrastic.uiEnableautosavestate; 
    ppcVar7[7] = (char *)0x100000000;
    puVar4[DRASTIC_MENU_OPTIONS_AUTO_SAVESTATE] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = base_addr_rx + 0x0015b430;//yes_no_labels.11545;
  
//
    ppcVar5 = (char **)malloc(0x48);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x40);
    }
    *(unsigned int  *)(ppcVar7 + 1) = 0x33+2;
    *ppcVar7 = "Assign auto savestate slot";
    ppcVar7[2] = (char *)draw_numeric;
    ppcVar7[3] = (char *)action_numeric;
    ppcVar7[4] = (char *)0x0;
    ppcVar7[5] = (char *)0x0;
    ppcVar5[6] = (char *)&g_advdrastic.uiSlotOfAutosavestate; 
    ppcVar5[7] = (char *)0xa00000000; // 0~10
    puVar4[DRASTIC_MENU_OPTIONS_AUTO_SAVESTATE_SLOT] = ppcVar5;
    ppcVar5[2] = (char *)draw_numeric_labeled;
    ppcVar5[8] = static_text_slots;
///

    ppcVar5 = (char **)malloc(0x38);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x30);
    }
    *ppcVar7 = "Delete game-specific config";
    *(unsigned int  *)(ppcVar7 + 1) = 0x34+2;
    ppcVar7[2] = (char *)draw_menu_option;
    ppcVar7[3] = (char *)action_select;
    ppcVar7[4] = (char *)0x0;
    ppcVar7[5] = (char *)0x0;
    puVar4[DRASTIC_MENU_OPTIONS_DELETE_GAME_SPECIFIC_CONFIG] = ppcVar5;
    ppcVar5[6] = (char *)select_delete_config_local;
  
    ppcVar5 = (char **)malloc(0x38);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x30);
    }
    *ppcVar7 = "Exit: save for all games";
    *(unsigned int  *)(ppcVar7 + 1) = 0x36+1;
    ppcVar7[2] = (char *)draw_menu_option;
    ppcVar7[3] = (char *)action_select;
    ppcVar7[4] = (char *)0x0;
    ppcVar7[5] = (char *)0x0;
    puVar4[DRASTIC_MENU_OPTIONS_EXIT_SAVE_FOR_ALL_GAMES] = ppcVar5;
    ppcVar5[6] = (char *)select_save_config_global;
  
    ppcVar5 = (char **)malloc(0x38);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x30);
    }
    *ppcVar7 = "Exit: save for this game";
    *(unsigned int  *)(ppcVar7 + 1) = 0x37+1;
    ppcVar7[2] = (char *)draw_menu_option;
    ppcVar7[3] = (char *)action_select;
    ppcVar7[4] = (char *)0x0;
    ppcVar7[5] = (char *)0x0;
    puVar4[DRASTIC_MENU_OPTIONS_EXIT_SAVE_FOR_THIS_GAME] = ppcVar5;
    ppcVar5[6] = (char *)select_save_config_local;
  
    ppcVar5 = (char **)malloc(0x38);
    ppcVar7 = ppcVar5;
    if (ppcVar5 == (char **)0x0) {
        ppcVar7 = (char **)malloc(0x30);
    }
    iVar1 = *(int *)(param_1 + 0x40);
    *ppcVar7 = "Exit without saving";
    *(unsigned int  *)(ppcVar7 + 1) = 0x38+1;
    ppcVar7[2] = (char *)draw_menu_option;
    ppcVar7[3] = (char *)action_select;
    ppcVar7[4] = (char *)0x0;
    ppcVar7[5] = (char *)0x0;
    puVar4[DRASTIC_MENU_OPTIONS_EXIT_WITHOUT_SAVING] = ppcVar5;
    uVar2 = 0x5c;
    if (iVar1 == 0) {
        uVar2 = 0x100;
    }
    *(unsigned int  *)(ppcVar3 + 2) = uVar2;
    //*(unsigned int  *)((long)ppcVar3 + 0x14) = 0x19;

//*(unsigned long *)((long)ppcVar3 + 0x14) = 0x19; //+1;  // 앞부분에서 정의 되어 있음.

    ppcVar5[6] = (char *)select_exit_current_menu;
    return ppcVar3;
}


const char cAdvance_control_name[NUM_OF_ADVANCE_CONTROL_INDEX][40] = {
    "Assign [Hot] key",
    "Vertical Up",
    "Vertical Down",
    "Vertical Left",
    "Vertical Right",
    "Change Layout(-) [Hot]",
    "Change Layout(+) [Hot]",
    "Toggle DPAD/Stylus",
    "Toggle DPAD/Stylus [Hot]",
    "Toggle Blur/Pixel [Hot]",
    "Change Theme [Hot]",
    "Enter Menu [Hot]",
    "Enter Custom setting [Hot]",
    "Toggle Fast Forward [Hot]",
    "Save State[Hot]",
    "Load State [Hot]",
    "Quit [Hot]"
};
    
/*
[trngaje] 0, Enter Menu          , 0x12
[trngaje] 1, Save State          , 0x13
[trngaje] 2, Load State          , 0x14
[trngaje] 3, Toggle Fast Forward , 0x15
[trngaje] 4, Swap Screens        , 0x16
[trngaje] 5, Swap Orientation A  , 0x17
[trngaje] 6, Swap Orientation B  , 0x18
[trngaje] 7, Load New Game       , 0x19
[trngaje] 8, Quit                , 0x1a
[trngaje] 9, Fake Microphone     , 0x1b
[trngaje] 10, Menu Cursor Up      , 0x1f
[trngaje] 11, Menu Cursor Down    , 0x20
[trngaje] 12, Menu Cursor Left    , 0x21
[trngaje] 13, Menu Cursor Right   , 0x22
[trngaje] 14, Menu Select         , 0x23
[trngaje] 15, Menu Exit Menu      , 0x24
[trngaje] 16, Menu Up Directory   , 0x25
[trngaje] 17, Menu Page Up        , 0x26
[trngaje] 18, Menu Page Down      , 0x27
[trngaje] 19, Menu Switch View    , 0x28
*/
unsigned char ** sdl_create_menu_extra_controls(long param_1, unsigned long param_2, unsigned char *param_3)
{
    int iVar1;
    long lVar2;
    uint uVar3;
    unsigned int uVar4;
    unsigned char **ppcVar5;
    unsigned char *pcVar6;
    unsigned long *puVar7;
    char **ppcVar8;
    char **ppcVar9;
    long lVar10;
    long lVar11;
    unsigned char *puVar12;
    long lVar13;
    // test
    //static unsigned short usinputa=0xffff;
    //static unsigned short usinputb=0xffff;
    
    nds_draw_button_config  draw_button_config;
    nds_action_button_config    action_button_config;
    nds_select_restore_default_controls select_restore_default_controls;
    nds_select_delete_config_local  select_delete_config_local;
    nds_select_save_config_global   select_save_config_global;
    nds_select_exit_current_menu    select_exit_current_menu;
    nds_draw_menu_option    draw_menu_option;
    nds_action_select   action_select;
    nds_draw_menu_controls  draw_menu_controls;
    nds_focus_menu_none focus_menu_none;
    
    long lAdvControlmenu_index;
    long lmenu_pos;
    
 
    
    draw_button_config = (nds_draw_button_config)FUN_DRAW_BUTTON_CONFIG;
    action_button_config = (nds_action_button_config)FUN_ACTION_BUTTON_CONFIG;
    select_restore_default_controls = (nds_select_restore_default_controls)FUN_SELECT_RESTORE_DEFAULT_CONTROLS;
    select_delete_config_local = (nds_select_delete_config_local)FUN_SELECT_DELETE_CONFIG_LOCAL;
    select_save_config_global = (nds_select_save_config_global)FUN_SELECT_SAVE_CONFIG_GLOBAL;
    select_exit_current_menu = (nds_select_exit_current_menu)FUN_SELECT_EXIT_CURRENT_MENU;
    draw_menu_option = (nds_draw_menu_option)FUN_DRAW_MENU_OPTION;
    action_select = (nds_action_select)FUN_ACTION_SELECT;   
    draw_menu_controls = (nds_draw_menu_controls)FUN_DRAW_MENU_CONTROLS;
    focus_menu_none = (nds_focus_menu_none)FUN_FOCUS_MENU_NONE;
    
    lVar11 = *(long *)(param_1 + 8);
    ppcVar5 = (unsigned char **)malloc(0x30);
    *(unsigned long *)((long)ppcVar5 + 0x14) = 0x17 + NUM_OF_ADVANCE_CONTROL_INDEX;
    *ppcVar5 = draw_menu_controls;
    ppcVar5[1] = focus_menu_none;
    lVar13 = 0;
    ppcVar5[5] = param_3;
    pcVar6 = (unsigned char *)malloc(0xc0+NUM_OF_ADVANCE_CONTROL_INDEX*8);
    ppcVar5[4] = pcVar6;
    do {
        //uVar3 = (&options_to_config_map.11558)[lVar13];
        //uVar3 = ((uint32_t **)(base_addr_rx + 0x001350b0))[lVar13];
        uVar3 = *(uint32_t *)(base_addr_rx + 0x001350b0 + lVar13*4);
        iVar1 = (int)lVar13 + 0x1e;
        //puVar12 = (&input_names.11557)[lVar13];
        //puVar12 = ((uint32_t **)(base_addr_rx + 0x00158cf0))[lVar13];
        puVar12 = *(uint64_t *)(base_addr_rx + 0x00158cf0+lVar13*8);
        //printf("[trngaje] %d, %s, 0x%x\n", lVar13, puVar12, uVar3);
        puVar7 = (unsigned long *)malloc(0x50);
        if (puVar7 == 0) {
            puVar7 = (unsigned long *)malloc(0x30);
        }
        
        lVar10 = ((ulong)uVar3 + 0x262) * 2;
        lVar2 = lVar11 + lVar10;
        lVar10 = lVar11 + lVar10 + 0x52;

        *(unsigned long *)(pcVar6 + lVar13 * 8) = 0;
        *(unsigned long **)(pcVar6 + lVar13 * 8) = puVar7;
        lVar13 = lVar13 + 1;
        *puVar7 = puVar12;                 // button name
        *(int *)(puVar7 + 1) = iVar1;  // button pos y
        puVar7[4] = 0;
        puVar7[5] = 0;
        puVar7[7] = lVar2;      // keyboard value
        puVar7[8] = lVar10;    // joy button value
        puVar7[2] = draw_button_config;
        puVar7[3] = action_button_config;
        *(unsigned char *)(puVar7 + 9) = 0;
    } while (lVar13 != 0x14);
    LAB_00092358:
    //--------------------------------------------
    
    lmenu_pos = 0x33;
    
    for(lAdvControlmenu_index=0; lAdvControlmenu_index<NUM_OF_ADVANCE_CONTROL_INDEX; lAdvControlmenu_index++) {
        ppcVar8 = (char **)malloc(0x50);
        ppcVar9 = ppcVar8;
        if (ppcVar8 == (char **)0x0) {
            ppcVar9 = (char **)malloc(0x30);
        }
        *ppcVar9 = cAdvance_control_name[lAdvControlmenu_index]; //"Hot key";
        *(unsigned int *)(ppcVar9 + 1) = 0x33+lAdvControlmenu_index;
        ppcVar9[2] = (char *)draw_button_config;
        ppcVar9[3] = (char *)action_button_config;
        ppcVar9[4] = (char *)0x0;
        ppcVar9[5] = (char *)0x0;
        ppcVar9[7] = (char *)&g_advControls[lAdvControlmenu_index].a;
        ppcVar9[8] = (char *)&g_advControls[lAdvControlmenu_index].b;
        *(unsigned char *)(ppcVar9+ 9) = 0;
        *(char ***)(pcVar6 + 0xa0+ 8*lAdvControlmenu_index) = ppcVar8;
    }
    //-----------------------------------------------------------------
    ppcVar8 = (char **)malloc(0x38);
    ppcVar9 = ppcVar8;
    if (ppcVar8 == (char **)0x0) {
        ppcVar9 = (char **)malloc(0x30);
    }
    *ppcVar9 = "Restore default controls";
    *(unsigned int *)(ppcVar9 + 1) = 0x33+NUM_OF_ADVANCE_CONTROL_INDEX;
    ppcVar9[2] = (char *)draw_menu_option;
    ppcVar9[3] = (char *)action_select;
    ppcVar9[4] = (char *)0x0;
    ppcVar9[5] = (char *)0x0;
    ppcVar8[6] = (char *)select_restore_default_controls;
    *(char ***)(pcVar6 + 0xa0+8*NUM_OF_ADVANCE_CONTROL_INDEX) = ppcVar8;
    ppcVar8 = (char **)malloc(0x38);
    ppcVar9 = ppcVar8;
    if (ppcVar8 == (char **)0x0) {
        ppcVar9 = (char **)malloc(0x30);
    }
    *ppcVar9 = "Delete game-specific config";
    *(unsigned int *)(ppcVar9 + 1) = 0x34+NUM_OF_ADVANCE_CONTROL_INDEX;
    ppcVar9[2] = (char *)draw_menu_option;
    ppcVar9[3] = (char *)action_select;
    ppcVar9[4] = (char *)0x0;
    ppcVar9[5] = (char *)0x0;
    ppcVar8[6] = (char *)select_delete_config_local;
    *(char ***)(pcVar6 + 0xa8+8*NUM_OF_ADVANCE_CONTROL_INDEX) = ppcVar8;
    ppcVar8 = (char **)malloc(0x38);
    ppcVar9 = ppcVar8;
    if (ppcVar8 == (char **)0x0) {
        ppcVar9 = (char **)malloc(0x30);
    }
    *ppcVar9 = "Exit: save for all games";
    *(unsigned int *)(ppcVar9 + 1) = 0x36+NUM_OF_ADVANCE_CONTROL_INDEX;
    ppcVar9[2] = (char *)draw_menu_option;
    ppcVar9[3] = (char *)action_select;
    ppcVar9[4] = (char *)0x0;
    ppcVar9[5] = (char *)0x0;
    ppcVar8[6] = (char *)select_save_config_global;
    *(char ***)(pcVar6 + 0xb0+8*NUM_OF_ADVANCE_CONTROL_INDEX) = ppcVar8;
    ppcVar8 = (char **)malloc(0x38);
    ppcVar9 = ppcVar8;
    if (ppcVar8 == (char **)0x0) {
        ppcVar9 = (char **)malloc(0x30);
    }
    *(char ***)(pcVar6 + 0xb8+8*NUM_OF_ADVANCE_CONTROL_INDEX) = ppcVar8;
    *ppcVar9 = "Exit without saving";
    *(unsigned int *)(ppcVar9 + 1) = 0x37+NUM_OF_ADVANCE_CONTROL_INDEX;
    ppcVar9[2] = (char *)draw_menu_option;
    ppcVar9[3] = (char *)action_select;
    ppcVar9[4] = (char *)0x0;
    ppcVar9[5] = (char *)0x0;
    ppcVar8[6] = (char *)select_exit_current_menu;
    *(unsigned int *)(param_1 + 0x164) = 0;
    uVar4 = 0xd8;
    if (*(int *)(param_1 + 0x40) != 0) {
        uVar4 = 0x38+1;
    }
    *(unsigned int *)(ppcVar5 + 2) = uVar4;
    return ppcVar5;
}

/*
[trngaje] sdl_draw_button_config: 30,D-Pad Up            , KB Up, JS0 Hat 01, 0x87d4ea2c, 0x87d4ea7e
[trngaje] sdl_draw_button_config: 31,D-Pad Down          , KB Down, JS0 Hat 04, 0x87d4ea2e, 0x87d4ea80
[trngaje] sdl_draw_button_config: 32,D-Pad Left          , KB Left, JS0 Hat 08, 0x87d4ea30, 0x87d4ea82
[trngaje] sdl_draw_button_config: 33,D-Pad Right         , KB Right, JS0 Hat 02, 0x87d4ea32, 0x87d4ea84
[trngaje] sdl_draw_button_config: 34,A                   , KB Space, JS0 Button 03, 0x87d4ea34, 0x87d4ea86
[trngaje] sdl_draw_button_config: 35,B                   , KB Left Ctrl, JS0 Button 04, 0x87d4ea36, 0x87d4ea88
[trngaje] sdl_draw_button_config: 36,X                   , KB Left Shift, JS0 Button 06, 0x87d4ea38, 0x87d4ea8a
[trngaje] sdl_draw_button_config: 37,Y                   , KB Left Alt, JS0 Button 05, 0x87d4ea3a, 0x87d4ea8c
[trngaje] sdl_draw_button_config: 38,L Trigger           , KB E, JS0 Button 07, 0x87d4ea3c, 0x87d4ea8e
[trngaje] sdl_draw_button_config: 39,R Trigger           , KB T, JS0 Button 08, 0x87d4ea3e, 0x87d4ea90
[trngaje] sdl_draw_button_config: 40,Start               , KB Return, JS0 Button 0a, 0x87d4ea40, 0x87d4ea92
[trngaje] sdl_draw_button_config: 41,Select              , KB Right Ctrl, JS0 Button 09, 0x87d4ea42, 0x87d4ea94
[trngaje] sdl_draw_button_config: 42,Hinge Shut          , Unmapped, Unmapped, 0x87d4ea44, 0x87d4ea96
[trngaje] sdl_draw_button_config: 43,Touch Cursor Up     , Unmapped, JS0 Axis- 01, 0x87d4ea46, 0x87d4ea98
[trngaje] sdl_draw_button_config: 44,Touch Cursor Down   , Unmapped, JS0 Axis+ 01, 0x87d4ea48, 0x87d4ea9a
[trngaje] sdl_draw_button_config: 45,Touch Cursor Left   , Unmapped, JS0 Axis- 00, 0x87d4ea4a, 0x87d4ea9c
[trngaje] sdl_draw_button_config: 46,Touch Cursor Right  , Unmapped, JS0 Axis+ 00, 0x87d4ea4c, 0x87d4ea9e
[trngaje] sdl_draw_button_config: 47,Touch Cursor Press  , Unmapped, JS0 Button 0f, 0x87d4ea4e, 0x87d4eaa0

[trngaje] sdl_draw_button_config: 30,Enter Menu          , KB Home, JS0 Button 0c, 0x82661a50, 0x82661aa2
[trngaje] sdl_draw_button_config: 31,Save State          , KB 0, Unmapped, 0x82661a52, 0x82661aa4
[trngaje] sdl_draw_button_config: 32,Load State          , KB 1, Unmapped, 0x82661a54, 0x82661aa6
[trngaje] sdl_draw_button_config: 33,Toggle Fast Forward , KB 2, Unmapped, 0x82661a56, 0x82661aa8
[trngaje] sdl_draw_button_config: 34,Swap Screens        , KB Backspace, JS0 Button 0e, 0x82661a58, 0x82661aaa
[trngaje] sdl_draw_button_config: 35,Swap Orientation A  , Unmapped, JS0 Button 0d, 0x82661a5a, 0x82661aac
[trngaje] sdl_draw_button_config: 36,Swap Orientation B  , Unmapped, Unmapped, 0x82661a5c, 0x82661aae
[trngaje] sdl_draw_button_config: 37,Load New Game       , Unmapped, Unmapped, 0x82661a5e, 0x82661ab0
[trngaje] sdl_draw_button_config: 38,Quit                , KB 3, Unmapped, 0x82661a60, 0x82661ab2
[trngaje] sdl_draw_button_config: 39,Fake Microphone     , Unmapped, JS0 Button 0b, 0x82661a62, 0x82661ab4
[trngaje] sdl_draw_button_config: 40,Menu Cursor Up      , KB Up, JS0 Hat 01, 0x82661a6a, 0x82661abc
[trngaje] sdl_draw_button_config: 41,Menu Cursor Down    , KB Down, JS0 Hat 04, 0x82661a6c, 0x82661abe
[trngaje] sdl_draw_button_config: 42,Menu Cursor Left    , KB Left, JS0 Hat 08, 0x82661a6e, 0x82661ac0
[trngaje] sdl_draw_button_config: 43,Menu Cursor Right   , KB Right, JS0 Hat 02, 0x82661a70, 0x82661ac2
[trngaje] sdl_draw_button_config: 44,Menu Select         , KB Space, JS0 Button 03, 0x82661a72, 0x82661ac4
[trngaje] sdl_draw_button_config: 45,Menu Exit Menu      , KB Left Ctrl, JS0 Button 04, 0x82661a74, 0x82661ac6
[trngaje] sdl_draw_button_config: 46,Menu Up Directory   , KB Backspace, JS0 Button 06, 0x82661a76, 0x82661ac8
[trngaje] sdl_draw_button_config: 47,Menu Page Up        , KB E, JS0 Button 07, 0x82661a78, 0x82661aca
[trngaje] sdl_draw_button_config: 48,Menu Page Down      , KB T, JS0 Button 08, 0x82661a7a, 0x82661acc
[trngaje] sdl_draw_button_config: 49,Menu Switch View    , KB Left Shift, JS0 Button 05, 0x82661a7c, 0x82661ace

*/
void sdl_draw_button_config(long param_1,char **param_2,int param_3)
{
    int iVar1;
    size_t sVar2;
    int iVar3;
    int iVar4;
    unsigned char auStack_48 [32];
    unsigned char auStack_28 [32];
    //long local_8;
    
    nds_platform_print_code     platform_print_code;
    nds_print_string                       print_string;
    
    platform_print_code  = (nds_platform_print_code)FUN_PLATFORM_PRINT_CODE;
    print_string                    = (nds_print_string)FUN_PRINT_STRING;
           
    iVar1 = *(int *)(*(long *)(param_1 + 0x10) + 0x10); // 56으로 동일한 값

    sVar2 = strlen(*param_2);
    platform_print_code(auStack_48,*(unsigned short *)param_2[7]);
    platform_print_code(auStack_28,*(unsigned short *)param_2[8]);
    iVar4 = 0;
    if (param_3 != 0) {
        iVar3 = 0x17;
        if (*(char *)(param_2 + 9) != '\0') {
            iVar3 = 0x600;
        }
        param_3 = 0;
        iVar4 = iVar3;
        if (*(int *)(param_1 + 0x164) == 0) {
            iVar4 = 0;
            param_3 = iVar3;
        }
    }
    iVar3 = ((int)sVar2 + 1) * 8;
    print_string(*param_2,0xffff,0,iVar1,*(int *)(param_2 + 1) << 3);
    print_string(auStack_48,0xffff,param_3,iVar3 + iVar1,*(int *)(param_2 + 1) << 3);
    print_string(auStack_28,0xffff,iVar4,iVar3 + 0x78 + iVar1,*(int *)(param_2 + 1) << 3);
    //printf("[trngaje] sdl_draw_button_config: %d,%s, %s, %s, 0x%x, 0x%x\n", *(int *)(param_2 + 1), *param_2, auStack_48, auStack_28, param_2[7], param_2[8]);
    // param_2[0] : button name
    // param_2[1] : button index (y pos)
    // param_2[7] : key value
    // param_2[8] : joy button value
}

uint sdl_action_button_config(long param_1,long param_2,uint *param_3)
{
    unsigned short *puVar1;
    ulong uVar2;
    uint uVar3;

    nds_clear_gui_actions                      clear_gui_actions;
    nds_platform_get_config_input   platform_get_config_input;
    nds_delay_us                                       delay_us;
    
    clear_gui_actions                   = (nds_clear_gui_actions)FUN_CLEAR_GUI_ACTIONS;
    platform_get_config_input = (nds_platform_get_config_input)FUN_PLATFORM_GET_CONFIG_INPUT;
    delay_us                                     = (nds_delay_us)FUN_DELAY_US;
    
    if (*(char *)(param_2 + 0x48) != '\0') {
        printf("[trngaje] here: 9th parameter not 0\r\n");
        // 키를 변경 하기 위해서 a 버튼을 누르면 여기로 진입함
        uVar2 = platform_get_config_input();
        if (((uint)(uVar2 >> 10) & 0x3fffff) != 0x3f) {
            puVar1 = *(unsigned short **)(param_2 + 0x38);// 7th : inputa 
            if (*(int *)(param_1 + 0x164) == 1) {
                puVar1 = *(unsigned short **)(param_2 + 0x40);// 8th : inputb
            }
            *puVar1 = (short)uVar2; // save
        }
        *(unsigned char *)(param_2 + 0x48) = 0;
        *(unsigned int *)(param_1 + 0x168) = 0;
        clear_gui_actions();
        delay_us(100000);
        return 0xb;
    }
    uVar3 = *param_3;
    if (uVar3 != 4) {
        if (uVar3 < 5) {
            if (1 < uVar3) {
                uVar3 = 0xb;
                *(uint *)(param_1 + 0x164) = *(uint *)(param_1 + 0x164) ^ 1;
            }
        }
        else if (uVar3 == 6) {
            puVar1 = *(unsigned short **)(param_2 + 0x38); // 7th : inputa 
            if (*(int *)(param_1 + 0x164) == 1) {
                puVar1 = *(unsigned short **)(param_2 + 0x40); // 8th : inputb
            }
            *puVar1 = 0xffff;
            return 0xb;
        }
        return uVar3;
    }
    *(unsigned char *)(param_2 + 0x48) = 1; // 9th
    *(unsigned int *)(param_1 + 0x168) = 1;
    return 0xb;
}

void sdl_savestate_pre(void)
{
#if 0 //ndef UNITTEST
    asm volatile (
        "mov r1, %0                 \n"
        "mov r2, #1                 \n"
        "str r2, [r1]               \n"
        "add sp, sp, #(0x20 + 0)    \n"
        "ldr pc, [sp], #4           \n"
        ::"r"(&savestate_busy):
    );
#endif
}

void sdl_savestate_post(void)
{
#if 0//ndef UNITTEST
    asm volatile (
        "mov r1, %0                 \n"
        "mov r2, #0                 \n"
        "str r2, [r1]               \n"
        "add sp, sp, #(0x18 + 0)    \n"
        "ldr pc, [sp], #4           \n"
        ::"r"(&savestate_busy):
    );
#endif
}
/*
[trngaje] sdl_spu_load_fake_microphone_data:/userdata/system/drastic/microphone/1062 - Picross DS (E)(Legacy).wav
[trngaje] sdl_spu_load_fake_microphone_data:/userdata/system/drastic/microphone/microphone.wav
*** buffer overflow detected ***: terminated : wav 구조체로 읽을 수 있도록 변경할 것
*/
unsigned long sdl_spu_load_fake_microphone_data(long param_1,char *param_2)
{
    FILE *__stream;
    ulong uVar1;
    size_t sVar2;
    void *__ptr;
    unsigned long uVar3;
    long lVar4;
    int local_30;
    uint local_2c;
    int local_28; // fread로 읽혀지는 버퍼 4byte x 3
    int local_24;
    int local_20;
    int local_18; // 4bytes x 4
  
    unsigned int local_14; // sample rate
    short local_a;
    long local_8;
  
    printf("[trngaje] sdl_spu_load_fake_microphone_data:%s\n", param_2);
    __stream = fopen(param_2,"rb");
    if (__stream == (FILE *)0x0) {
        uVar3 = 0xffffffff;
    }
    else {
        fseek(__stream,0,2); // int fseek(FILE* stream, long int offset, int origin);
        uVar1 = ftell(__stream);
        fseek(__stream,0,0);
        sVar2 = fread(&local_28,4,3,__stream); // size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
        if (sVar2 == 3) {
            // header 정보
            // https://crystalcube.co.kr/123

            if (local_28 == 0x46464952) { // "RIFF"
                if (local_24 == (int)uVar1 + -8) {
                    if (local_20 == 0x45564157) { // "WAVE"
                        do {
                            sVar2 = fread(&local_30,4,2,__stream);
                            if (sVar2 != 2) {
                                puts(" ERROR: Incomplete sub-chunk header.");
                                goto LAB_0007f438;
                            }
                            if (local_30 == 0x20746d66) { // "fmt "
                                if (local_2c != 0x10) {
                                    puts(" ERROR: WAV fmt chunk is incorrectly sized.");
                                    goto LAB_0007f438;
                                }
                                sVar2 = fread(&local_18,4,4,__stream);
                                if (sVar2 != 4) {
                                    puts(" ERROR: Could not read fmt data from WAV file.");
                                    goto LAB_0007f438;
                                }
                                if (local_18 != 0x10001) { // audio format (2byte) | numchannels (2byte)
                                    __printf_chk(1," ERROR: WAV must be uncompressed PCM and mono (is %08x).\n");
                                    goto LAB_0007f438;
                                }
                                if (local_a != 0x10) { // bit per sample (2byte)
                                    puts(" ERROR: WAV must be 16 bits per sample.");
                                    goto LAB_0007f438;
                                }
                            }
                            else {
                                if (local_30 == 0x61746164) { //"data"
                                    uVar1 = (ulong)local_2c; // Chunk Size
                                    __ptr = malloc(uVar1);
                                    *(void **)(param_1 + 0x40d30) = __ptr;
                                    sVar2 = fread(__ptr,uVar1,1,__stream);
                                    if (sVar2 == 1) {
                                        uVar3 = 0;
                                        *(unsigned int *)(param_1 + 0x40d38) = local_14;
                                        *(uint *)(param_1 + 0x40d3c) = local_2c >> 1;
                                        goto LAB_0007f384;
                                    }
                                    puts(" ERROR: Could not read WAV data.");
                                    free(*(void **)(param_1 + 0x40d30));
                                    *(unsigned long *)(param_1 + 0x40d30) = 0;
                                    goto LAB_0007f438;
                                }
                                fseek(__stream,(ulong)local_2c,1);
                            }
                            lVar4 = ftell(__stream);
                        } while (lVar4 < (long)(uVar1 & 0xffffffff));
                        puts("ERROR: Did not find data subchunk in WAV file.");
                    }
                    else {
                        puts(" ERROR: WAV file does not have correct Format string.");
                    }
                }
                else {
                    __printf_chk(1," ERROR: WAV file does not have correct ChunkSize (%d should be %d)\n");
                }
            }
            else {
                puts(" ERROR: WAV file does not have correct ChunkID string.");
            }
        }
        else {
            __printf_chk(1," ERROR: Could not read WAV header from %s.\n",param_2);
        }
LAB_0007f438:
        fclose(__stream);
        uVar3 = 0xffffffff;
    }
    
LAB_0007f384:
    return uVar3;
}

void sigterm_handler(int sig)
{
    static int ran = 0;

    if (ran == 0) {
        ran = 1;
        printf(PREFIX"Oops sigterm !\n");
        dtr_quit();
    }
}

static void strip_newline(char *p)
{
    int cc = 0, len = strlen(p);

    for(cc=0; cc<len; cc++) {
        if ((p[cc] == '\r') || (p[cc] == '\n')) {
            p[cc] = 0;
            break;
        }
    }
}

static void *video_handler(void *threadid)
{
    time_t T;                                                                     
    struct timespec t; 
    int result;
    
    egl_init();
 

    // acquire a lock
    pthread_mutex_lock(&lock);
        
    while (is_running) {
        time(&T);
        t.tv_sec = T + 1;
        
        result = pthread_cond_wait(&request_update_screen_cond, &lock);
        //result = pthread_cond_timedwait(&request_update_screen_cond, &lock, &t); // 프로그램 종료시 loop를 빠져나갈 수 있도록 추가
        if (result != ETIMEDOUT && result != EINTR && is_running != 0) {            
            if (nds.menu.enable) {
                if (nds.update_menu) {
                    nds.update_menu = 0;
                    GFX_Copy(-1, cvt->pixels, cvt->clip_rect, cvt->clip_rect, cvt->pitch, 0, E_MI_GFX_ROTATE_180);
                    GFX_Flip();
                }
            }
            else if (nds.menu.drastic.enable) {
                printf("[trngaje] nds.menu.drastic.enable:true, nds.update_menu=%d\n", nds.update_menu);
                if (nds.update_menu) {
                    nds.update_menu = 0;
                    GFX_Copy(-1, nds.menu.drastic.main->pixels, nds.menu.drastic.main->clip_rect, nds.menu.drastic.main->clip_rect, nds.menu.drastic.main->pitch, 0, 0);
                    GFX_Flip();
                }
           }
           else if (nds.update_screen) {
               static int frame_dbg = 0;
               
               // Try to initialize EGL if not done yet
               if (!egl_initialized) {
                   egl_init();
               }
               
               if (frame_dbg < 5) {
                   printf("[DEBUG] video_handler: process_screen frame %d, pixels[0]=%p, pixels[1]=%p, egl_init=%d\n",
                          frame_dbg, (void*)nds.screen.pixels[0], (void*)nds.screen.pixels[1], egl_initialized);
                   frame_dbg++;
               }
               
               if (egl_initialized) {
                   process_screen();
                   GFX_Flip();
               }
               nds.update_screen = 0;
           } 
           else {
               usleep(0);
           }
           pthread_cond_signal(&response_update_screen_cond);
        }
    }

    // release lock 
    pthread_mutex_unlock(&lock); 
    
    pthread_exit(NULL);
}

static int lang_unload(void)
{
    int cc = 0;

    for (cc=0; translate[cc]; cc++) {
        if (translate[cc]) {
            free(translate[cc]);
        }
        translate[cc] = NULL;
    }
    memset(translate, 0, sizeof(translate));
    return 0;
}

static int lang_load(const char *lang)
{
    FILE *f = NULL;
    char buf[MAX_PATH << 1] = {0};

    if (strcasecmp(nds.lang.trans[DEF_LANG_SLOT], DEF_LANG_LANG)) {
        sprintf(buf, "%s/%s", nds.lang.path, lang);
        f = fopen(buf, "r");

        if (f != NULL) {
            int cc = 0, len = 0;

            memset(buf, 0, sizeof(buf));
            while (fgets(buf, sizeof(buf), f)) {
                strip_newline(buf);
                len = strlen(buf) + 2;
                if (len == 0) {
                    continue;
                }

                if (translate[cc] != NULL) {
                    free(translate[cc]);
                }
                translate[cc] = malloc(len);
                if (translate[cc] != NULL) {
                    memcpy(translate[cc], buf, len);
                    //printf(PREFIX"Translate: \'%s\'(len=%d)\n", translate[cc], len);
                }
                cc+= 1;
                if (cc >= MAX_LANG_LINE) {
                    break;
                }
                memset(buf, 0, sizeof(buf));
            }
            fclose(f);
        }
        else {
            printf(PREFIX"Failed to open lang folder \'%s\'\n", nds.lang.path);
        }
    }
    return 0;
}

static void lang_enum(void)
{
    int idx = 2;
    DIR *d = NULL;
    struct dirent *dir = NULL;

    strcpy(nds.lang.trans[DEF_LANG_SLOT], DEF_LANG_LANG);
    strcpy(nds.lang.trans[DEF_LANG_SLOT + 1], DEF_LANG_LANG);
    d = opendir(nds.lang.path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_DIR) {
                continue;
            }
            if (strcmp(dir->d_name, ".") == 0) {
                continue;
            }
            if (strcmp(dir->d_name, "..") == 0) {
                continue;
            }

            //printf(PREFIX"found lang \'lang[%d]=%s\'\n", idx, dir->d_name);
            strcpy(nds.lang.trans[idx], dir->d_name);
            idx+= 1;
            if (idx >= MAX_LANG_FILE) {
                break;
            }
        }
        closedir(d);
    }
}

static int read_config(void)
{
    struct json_object *jval = NULL;
    struct json_object *jfile = NULL;
/*
	const char mykey_name[NUM_OF_MYKEY][20]= {
        "up", "down", "left", "right", "a", "b", "x", "y", "l1", "r1", 
        "l2", "r2", "l3", "r3","select", "start", "menu", "qsave", "qload", "ff", 
		"exit", "menu_onion"
    };
	
	char full_mykey_name[100];
*/
	int i;

    jfile = json_object_from_file(nds.cfg.path);
    if (jfile == NULL) {
        printf(PREFIX"Failed to read settings from json file (%s)\n", nds.cfg.path);
        return -1;
    }

    json_object_object_get_ex(jfile, JSON_NDS_PEN_SEL, &jval);
    if (jval) {
        nds.pen.sel = json_object_get_int(jval);
    }
/*
    json_object_object_get_ex(jfile, JSON_NDS_PEN_POS, &jval);
    if (jval) {
        nds.pen.pos = json_object_get_int(jval) == 0 ? 0 : 1;
    }
*/
    json_object_object_get_ex(jfile, JSON_NDS_THEME_SEL, &jval);
    if (jval) {
        nds.theme.sel = json_object_get_int(jval);
    }

    json_object_object_get_ex(jfile, JSON_NDS_DIS_MODE, &jval);
    if (jval) {
        g_advdrastic.ucLayoutIndex[0]/*nds.dis_mode*/ = json_object_get_int(jval);
        // 입력된 값이 범위를 벗어난지 검사
         if (g_advdrastic.ucLayoutIndex[0] > (getmax_layout(0))) {
            g_advdrastic.ucLayoutIndex[0] = 0;
        }       
    }

    nds.dis_hres_mode = (nds.dis_mode == NDS_DIS_MODE_HRES1) ? NDS_DIS_MODE_HRES1 : NDS_DIS_MODE_HRES0;
/*
    json_object_object_get_ex(jfile, JSON_NDS_ALT_MODE, &jval);
    if (jval) {
        nds.alt_mode = json_object_get_int(jval);
    }
  
    json_object_object_get_ex(jfile, JSON_NDS_PEN_XV, &jval);
    if (jval) {
        nds.pen.xv = json_object_get_int(jval);
    }
    
    json_object_object_get_ex(jfile, JSON_NDS_PEN_YV, &jval);
    if (jval) {
        nds.pen.yv = json_object_get_int(jval);
    }
  */
    json_object_object_get_ex(jfile, JSON_NDS_ALPHA_VALUE, &jval);
    if (jval) {
        nds.alpha.val = json_object_get_int(jval);
    }

    json_object_object_get_ex(jfile, JSON_NDS_ALPHA_POSITION, &jval);
    if (jval) {
        nds.alpha.pos = json_object_get_int(jval);
    }

    json_object_object_get_ex(jfile, JSON_NDS_ALPHA_BORDER, &jval);
    if (jval) {
        nds.alpha.border = json_object_get_int(jval);
        nds.alpha.border%= NDS_BORDER_MAX;
    }
/*
    json_object_object_get_ex(jfile, JSON_NDS_MAX_CPU, &jval);
    if (jval) {
        nds.maxcpu = json_object_get_int(jval);
    }

    json_object_object_get_ex(jfile, JSON_NDS_MIN_CPU, &jval);
    if (jval) {
        nds.mincpu = json_object_get_int(jval);
    }

    json_object_object_get_ex(jfile, JSON_NDS_MAX_CORE, &jval);
    if (jval) {
        nds.maxcore = json_object_get_int(jval);
    }

    json_object_object_get_ex(jfile, JSON_NDS_MIN_CORE, &jval);
    if (jval) {
        nds.mincore = json_object_get_int(jval);
    }
*/
    json_object_object_get_ex(jfile, JSON_NDS_OVERLAY, &jval);
    if (jval) {
        nds.overlay.sel = json_object_get_int(jval);
    }
#if 0
    json_object_object_get_ex(jfile, JSON_NDS_SWAP_L1L2, &jval);
    if (jval) {
        nds.swap_l1l2 = json_object_get_int(jval) ? 1 : 0;
    }

    json_object_object_get_ex(jfile, JSON_NDS_SWAP_R1R2, &jval);
    if (jval) {
        nds.swap_r1r2 = json_object_get_int(jval) ? 1 : 0;
    }


    json_object_object_get_ex(jfile, JSON_NDS_KEYS_ROTATE, &jval);
    if (jval) {
        nds.keys_rotate = json_object_get_int(jval) % 3;
    }
#endif


    nds.menu.c0 = 0xffffff;
    json_object_object_get_ex(jfile, JSON_NDS_MENU_C0, &jval);
    if (jval) {
        const char *p = json_object_get_string(jval);
        nds.menu.c0 = strtol(p, NULL, 16);
    }

    nds.menu.c1 = 0x000000;
    json_object_object_get_ex(jfile, JSON_NDS_MENU_C1, &jval);
    if (jval) {
        const char *p = json_object_get_string(jval);
        nds.menu.c1 = strtol(p, NULL, 16);
    }

    nds.menu.c2 = 0x289a35;
    json_object_object_get_ex(jfile, JSON_NDS_MENU_C2, &jval);
    if (jval) {
        const char *p = json_object_get_string(jval);
        nds.menu.c2 = strtol(p, NULL, 16);
    }
/*
    nds.auto_state = 1;
    json_object_object_get_ex(jfile, JSON_NDS_AUTO_STATE, &jval);
    if (jval) {
        nds.auto_state = json_object_get_int(jval) ? 1 : 0;
    }

    nds.auto_slot = 1;
    json_object_object_get_ex(jfile, JSON_NDS_AUTO_SLOT, &jval);
    if (jval) {
        nds.auto_slot = json_object_get_int(jval);
    }
*/
    lang_enum();
    json_object_object_get_ex(jfile, JSON_NDS_LANG, &jval);
    if (jval) {
        const char *lang = json_object_get_string(jval);

        strcpy(nds.lang.trans[DEF_LANG_SLOT], lang);
        lang_load(lang);
    }
/*
    json_object_object_get_ex(jfile, JSON_NDS_HOTKEY, &jval);
    if (jval) {
        nds.hotkey = json_object_get_int(jval);
    }
*/
    nds.menu.show_cursor = 0;
    json_object_object_get_ex(jfile, JSON_NDS_MENU_CURSOR, &jval);
    if (jval) {
        nds.menu.show_cursor = json_object_get_int(jval);
    }

/*
    nds.fast_forward = 6;
    json_object_object_get_ex(jfile, JSON_NDS_FAST_FORWARD, &jval);
    if (jval) {
        nds.fast_forward = json_object_get_int(jval);
    }
*/
    json_object_object_get_ex(jfile, JSON_NDS_STATES, &jval);
    if (jval) {
        struct stat st = {0};
        const char *path = json_object_get_string(jval);

        if ((path != NULL) && (path[0] != 0)) {
            strcpy(nds.states.path, path);

            if (stat(nds.states.path, &st) == -1) {
                mkdir(nds.states.path, 0755);
                printf(PREFIX"Create states folder in \'%s\'\n", nds.states.path);
            }
        }
    }
/*
    if (nds.dis_mode > NDS_DIS_MODE_LAST) {
        nds.dis_mode = NDS_DIS_MODE_VH_S0;
        nds.alt_mode = NDS_DIS_MODE_S0;
    }
*/
    nds.menu.sel = 0;
    json_object_object_get_ex(jfile, JSON_NDS_MENU_BG, &jval);
    if (jval) {
        nds.menu.sel = json_object_get_int(jval);
        if (nds.menu.sel >= nds.menu.max) {
            nds.menu.sel = 0;
        }
    }
/*	
	json_object_object_get_ex(jfile, "input_dev", &jval);
	if (jval) {
        const char *dev = json_object_get_string(jval);

        strcpy(nds.input.dev, dev);		
	}


	for (i=0; i< NUM_OF_MYKEY; i++) {
		sprintf(full_mykey_name, "input_%s", mykey_name[i]);
		json_object_object_get_ex(jfile, full_mykey_name, &jval);
		if (jval)
			nds.input.key[i] = json_object_get_int(jval);
		else
			nds.input.key[i] =  -1; // not defined
	}
*/	
	json_object_object_get_ex(jfile, "display_rotate", &jval);
	if (jval)
		nds.display.rotate = json_object_get_int(jval);
	else 
		nds.display.rotate = 0;
		
    json_object_object_get_ex(jfile, "pixel_filter", &jval);
    if (jval) {
        pixel_filter = json_object_get_int(jval);
    	if (pixel_filter >= 1)
    	    pixel_filter = 1;
    	else
    	    pixel_filter = 0;
    }
    
    reload_menu();

    reload_pen();
    //reload_overlay();

    json_object_put(jfile);

    return 0;
}

static int write_config(void)
{
    struct json_object *jfile = NULL;

    jfile = json_object_from_file(nds.cfg.path);
    if (jfile == NULL) {
        printf(PREFIX"Failed to write settings to json file (%s)\n", nds.cfg.path);
        return -1;
    }

/*
    if (nds.dis_mode > NDS_DIS_MODE_LAST) {
        nds.dis_mode = NDS_DIS_MODE_VH_S0;
        nds.alt_mode = NDS_DIS_MODE_S0;
    }
*/
    json_object_object_add(jfile, JSON_NDS_PEN_SEL, json_object_new_int(nds.pen.sel));
    json_object_object_add(jfile, JSON_NDS_THEME_SEL, json_object_new_int(nds.theme.sel));
    //json_object_object_add(jfile, JSON_NDS_DIS_MODE, json_object_new_int(nds.dis_mode));
    json_object_object_add(jfile, JSON_NDS_DIS_MODE, json_object_new_int(g_advdrastic.ucLayoutIndex[0]));
    json_object_object_add(jfile, JSON_NDS_ALPHA_VALUE, json_object_new_int(nds.alpha.val));
    json_object_object_add(jfile, JSON_NDS_ALPHA_POSITION, json_object_new_int(nds.alpha.pos));
    json_object_object_add(jfile, JSON_NDS_ALPHA_BORDER, json_object_new_int(nds.alpha.border));
    json_object_object_add(jfile, JSON_NDS_OVERLAY, json_object_new_int(nds.overlay.sel));
   // json_object_object_add(jfile, JSON_NDS_ALT_MODE, json_object_new_int(nds.alt_mode));
#if 0 
    json_object_object_add(jfile, JSON_NDS_KEYS_ROTATE, json_object_new_int(nds.keys_rotate));
#endif
    json_object_object_add(jfile, JSON_NDS_LANG, json_object_new_string(nds.lang.trans[DEF_LANG_SLOT]));
   // json_object_object_add(jfile, JSON_NDS_HOTKEY, json_object_new_int(nds.hotkey));
    //json_object_object_add(jfile, JSON_NDS_SWAP_L1L2, json_object_new_int(nds.swap_l1l2));
    //json_object_object_add(jfile, JSON_NDS_SWAP_R1R2, json_object_new_int(nds.swap_r1r2));
    //json_object_object_add(jfile, JSON_NDS_PEN_XV, json_object_new_int(nds.pen.xv));
    //json_object_object_add(jfile, JSON_NDS_PEN_YV, json_object_new_int(nds.pen.yv));
    json_object_object_add(jfile, JSON_NDS_MENU_BG, json_object_new_int(nds.menu.sel));
    json_object_object_add(jfile, JSON_NDS_MENU_CURSOR, json_object_new_int(nds.menu.show_cursor));
    //json_object_object_add(jfile, JSON_NDS_FAST_FORWARD, json_object_new_int(nds.fast_forward));

    json_object_object_add(jfile, JSON_NDS_PIXEL_FILTER, json_object_new_int(pixel_filter));

    json_object_to_file_ext(nds.cfg.path, jfile, JSON_C_TO_STRING_PRETTY);
    json_object_put(jfile);
    printf(PREFIX"Wrote changed settings back !\n");
    return 0;
}

int get_dir_path(const char *path, int desire, char *buf)
{
    DIR *d = NULL;
    int count = 0, r = -1;
    struct dirent *dir = NULL;

    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0) {
                continue;
            }

            if (strcmp(dir->d_name, "..") == 0) {
                continue;
            }

            if (dir->d_type != DT_DIR) {
                continue;
            }

            if (count == desire) {
                r = snprintf(buf, MAX_PATH, "%s/%s", path, dir->d_name) ? 0 : 1;
                break;
            }
            count+= 1;
        }
        closedir(d);
    }
    return r;
}

static int get_file_path(const char *path, int desire, char *buf, int add_path)
{
    DIR *d = NULL;
    int count = 0, r = -1;
    struct dirent *dir = NULL;

    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0) {
                continue;
            }

            if (strcmp(dir->d_name, "..") == 0) {
                continue;
            }

            if (dir->d_type == DT_DIR) {
                continue;
            }

            if (count == desire) {
                if (add_path) {
                    r = snprintf(buf, MAX_PATH, "%s/%s", path, dir->d_name) ? 0 : 1;
                }
                else {
                    r = snprintf(buf, MAX_PATH, "%s", dir->d_name) ? 0 : 1;
                }
                break;
            }
            count+= 1;
        }
        closedir(d);
    }
    return r;
}

static int get_dir_count(const char *path)
{
    DIR *d = NULL;
    int count = 0;
    struct dirent *dir = NULL;

    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type != DT_DIR) {
                continue;
            }
            if (strcmp(dir->d_name, ".") == 0) {
                continue;
            }
            if (strcmp(dir->d_name, "..") == 0) {
                continue;
            }
            count+= 1;
        }
        closedir(d);
    }
    return count;
}

static int get_file_count(const char *path)
{
    DIR *d = NULL;
    int count = 0;
    struct dirent *dir = NULL;

    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_DIR) {
                continue;
            }
            if (strcmp(dir->d_name, ".") == 0) {
                continue;
            }
            if (strcmp(dir->d_name, "..") == 0) {
                continue;
            }
            count+= 1;
        }
        closedir(d);
    }
    return count;
}

static int get_theme_count(void)
{
    return get_dir_count(nds.theme.path);
}

static int get_menu_count(void)
{
    return get_dir_count(MENU_PATH);
}

static int get_pen_count(void)
{
    return get_file_count(nds.pen.path);
}

static int get_overlay_count(void)
{
    return get_file_count(nds.overlay.path);
}

#ifdef ADVDRASTIC_DRM

uint64_t page_size = 4096;
uint64_t aligned_offset = 0;
size_t map_size = 0;
static int drm_create_fb(struct drm_device *bo)
{
	/* create a dumb-buffer, the pixel format is XRGB888 */
	bo->create.width = bo->width;
	bo->create.height = bo->height;
	bo->create.bpp = 32;

	/* handle, pitch, size will be returned */
	drmIoctl(dri_fd, DRM_IOCTL_MODE_CREATE_DUMB, &bo->create);

	/* bind the dumb-buffer to an FB object */
	bo->pitch = bo->create.pitch;
	bo->size = bo->create.size;
	bo->handle = bo->create.handle;
	drmModeAddFB(dri_fd, bo->width, bo->height, 24, 32, bo->pitch,
			   bo->handle, &bo->fb_id);

	printf("pitch = %d ,size = %d, handle = %d \n",bo->pitch,bo->size,bo->handle);

	/* map the dumb-buffer to userspace */
	bo->map.handle = bo->create.handle;
	drmIoctl(dri_fd, DRM_IOCTL_MODE_MAP_DUMB, &bo->map);


     	//Solve: -D_FILE_OFFSET_BITS=64
	bo->vaddr = (uint32_t *)mmap(0, bo->create.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, dri_fd, bo->map.offset);
     	//mmap failed: Invalid argument

        printf("Parameters: size=%d, offset=%llx, fd=%d\n", 
               bo->size, bo->map.offset, dri_fd);
    	if (bo->vaddr == MAP_FAILED) {
        	printf("mmap failed: %s\n", strerror(errno));
        	printf("Parameters: size=%d, offset=%llx, fd=%d\n", 
          	     bo->size, bo->map.offset, dri_fd);
       		return -1;
    	}
	/* initialize the dumb-buffer with white-color */
	memset(bo->vaddr, 0x0,bo->size);

	return 0;
}

static void drm_destroy_fb(struct drm_device *bo)
{
	struct drm_mode_destroy_dumb destroy = {};
	drmModeRmFB(dri_fd, bo->fb_id);
	munmap(bo->vaddr, bo->size);
	destroy.handle = bo->handle;
	drmIoctl(dri_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

int drm_init()
{
	// Open Dri
	dri_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if(dri_fd < 0){
		printf("dri wrong\n");
		return -1;
	}
        printf("fd flags: 0x%x\n", fcntl(dri_fd, F_GETFL));

	res = drmModeGetResources(dri_fd);
	crtc_id = res->crtcs[0];
	conn_id = res->connectors[1];
	printf("crtc = %d , conneter = %d\n",crtc_id,conn_id);

	conn = drmModeGetConnector(dri_fd, conn_id);
	drm_buf.width = conn->modes[0].hdisplay;
	drm_buf.height = conn->modes[0].vdisplay;

	printf("width = %d , height = %d\n",drm_buf.width,drm_buf.height);

	drm_create_fb(&drm_buf);
	
	// Init GBM Device
	drm_buf.gbm = gbm_create_device(dri_fd);
	if (!drm_buf.gbm) {
		fprintf(stderr, "Cannot create gbm device\n");
		return -1;
	}
	drm_buf.gbm_surface = gbm_surface_create(
		drm_buf.gbm,
		640,
		480,
		GBM_FORMAT_ARGB8888,
		GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING
	);

	if (!drm_buf.gbm_surface) {
		fprintf(stderr, "Cannot create gbm surface\n");
		return -1;
	}
	//Set CRTCS
	drmModeSetCrtc(dri_fd, crtc_id, drm_buf.fb_id,
			0, 0, &conn_id, 1, &conn->modes[0]);

	return 0;
}
void drm_exit()
{
	drm_destroy_fb(&drm_buf);
	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
	close(dri_fd);
}
#endif

int fb_init(void)
{
    gfx.fb_dev = open("/dev/fb0", O_RDWR, 0);
    if (gfx.fb_dev < 0) {
        printf(PREFIX"Failed to open /dev/fb0\n");
        return -1;
    }

    if (ioctl(gfx.fb_dev, FBIOGET_VSCREENINFO, &gfx.vinfo) < 0) {
        printf(PREFIX"Failed to get fb info\n");
        return -1;
    }
    gfx.vinfo.yres_virtual = gfx.vinfo.yres * 2;
    ioctl(gfx.fb_dev, FBIOPUT_VSCREENINFO, &gfx.vinfo);

// [trngaje] fb_init: xres=640, yres=480
// [trngaje] fb_init: 0x0, rotate=0

    // https://docs.huihoo.com/doxygen/linux/kernel/3.7/structfb__var__screeninfo.html
    printf("[trngaje] fb_init: xres=%d, yres=%d\n", gfx.vinfo.xres, gfx.vinfo.yres);
    printf("[trngaje] fb_init: %dx%d, rotate=%d\n", gfx.vinfo.width, gfx.vinfo.height, gfx.vinfo.rotate);

    return 0;
}

int fb_quit(void)
{
    if (gfx.fb_dev > 0) {
        close(gfx.fb_dev);
        gfx.fb_dev = -1;
    }

    return 0;
}

void egl_init(void)
{
    printf("[DEBUG] egl_init() called\n");
    fflush(stdout);
    
    if (egl_initialized) {
        printf("[DEBUG] EGL already initialized\n");
        fflush(stdout);
        return;
    }
    
#ifdef ADVDRASTIC_DRM
    EGLint egl_major = 0;
    EGLint egl_minor = 0;
    EGLint num_configs = 0;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   8,  
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,  
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLint const context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };

    vid.eglDisplay = eglGetDisplay((EGLNativeDisplayType)drm_buf.gbm);
    if (vid.eglDisplay == EGL_NO_DISPLAY) {
        printf("Failed to get EGL display\n");
        return;
    }

    if (!eglInitialize(vid.eglDisplay, NULL, NULL)) {
        printf("Failed to initialize EGL display\n");
        return;
    }
    
    eglChooseConfig(vid.eglDisplay, config_attribs, &vid.eglConfig, 1, &num_configs);
    
    vid.eglSurface = eglCreateWindowSurface(vid.eglDisplay, vid.eglConfig, (EGLNativeWindowType)drm_buf.gbm_surface, NULL);
    if (vid.eglSurface == EGL_NO_SURFACE) {
        EGLint error = eglGetError();
        printf("Failed to create EGL surface, error: 0x%x\n", error);
        return;
    }
    
    vid.eglContext = eglCreateContext(vid.eglDisplay, vid.eglConfig, EGL_NO_CONTEXT, context_attributes);
    eglMakeCurrent(vid.eglDisplay, vid.eglSurface, vid.eglSurface, vid.eglContext);
    printf("[DEBUG] EGL DRM context ready\n");
    fflush(stdout);
    egl_initialized = 1;
#else
    // Try to find an existing SDL window
    SDL_Window *window = NULL;
    
    // First check vid.window
    if (vid.window != NULL) {
        window = vid.window;
        printf("[DEBUG] Using vid.window: %p\n", window);
    } else {
        // Try to get window by ID (drastic typically uses window ID 1)
        window = SDL_GetWindowFromID(1);
        if (window != NULL) {
            printf("[DEBUG] Found window by ID 1: %p\n", window);
        }
    }
    
    if (window == NULL) {
        printf("[DEBUG] No SDL window available yet, deferring initialization\n");
        fflush(stdout);
        return;
    }
    
    found_window = window;
    {
        int win_w = 0, win_h = 0;
        SDL_GetWindowSize(window, &win_w, &win_h);
        printf("[DEBUG] SDL window: %p (%dx%d)\n", window, win_w, win_h);
    }
    fflush(stdout);
    
    // Set OpenGL ES 2.0 attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    
    // Create SDL GL context
    sdl_gl_context = SDL_GL_CreateContext(window);
    if (sdl_gl_context == NULL) {
        printf("[DEBUG] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        fflush(stdout);
        return;
    }
    
    printf("[DEBUG] SDL GL context created successfully\n");
    fflush(stdout);
    
    // Get EGL handles from current context
    vid.eglDisplay = eglGetCurrentDisplay();
    vid.eglContext = eglGetCurrentContext();
    vid.eglSurface = eglGetCurrentSurface(EGL_DRAW);
    
    printf("[DEBUG] EGL Display: %p, Context: %p, Surface: %p\n", 
           vid.eglDisplay, vid.eglContext, vid.eglSurface);
    fflush(stdout);
    
    SDL_GL_SetSwapInterval(1);
    egl_initialized = 1;
#endif

    // Setup shaders only if EGL was initialized successfully
    if (!egl_initialized) {
        return;
    }
  
    vid.vShader = glCreateShader(GL_VERTEX_SHADER);

	if (nds.display.rotate == 90)
		 glShaderSource(vid.vShader, 1, &vShaderDegree90Src, NULL);
	else if (nds.display.rotate == 180)
		 glShaderSource(vid.vShader, 1, &vShaderDegree180Src, NULL);
	else if (nds.display.rotate == 270)
		 glShaderSource(vid.vShader, 1, &vShaderDegree270Src, NULL);
	else
		glShaderSource(vid.vShader, 1, &vShaderSrc, NULL);


    glCompileShader(vid.vShader);
  
    vid.fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vid.fShader, 1, &fShaderSrc, NULL);
    glCompileShader(vid.fShader);
   
    vid.pObject = glCreateProgram();
    glAttachShader(vid.pObject, vid.vShader);
    glAttachShader(vid.pObject, vid.fShader);
    glLinkProgram(vid.pObject);
    glUseProgram(vid.pObject);

    if (vid.eglDisplay != EGL_NO_DISPLAY) {
        eglSwapInterval(vid.eglDisplay, 1);
    }
    vid.posLoc = glGetAttribLocation(vid.pObject, "a_position");
    vid.texLoc = glGetAttribLocation(vid.pObject, "a_texCoord");
    vid.samLoc = glGetUniformLocation(vid.pObject, "s_texture");
    vid.alphaLoc = glGetUniformLocation(vid.pObject, "s_alpha");

    glGenTextures(TEX_MAX, vid.texID);

	if (nds.display.rotate == 90 || nds.display.rotate == 270)
		glViewport(0, 0, g_advdrastic.iDisplay_height, g_advdrastic.iDisplay_width);
	else 
		glViewport(0, 0, g_advdrastic.iDisplay_width, g_advdrastic.iDisplay_height);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnableVertexAttribArray(vid.posLoc);
    glEnableVertexAttribArray(vid.texLoc);
    glUniform1i(vid.samLoc, 0);
    glUniform1f(vid.alphaLoc, 0.0);
    
    printf("[DEBUG] egl_init() completed, viewport: %dx%d\n", g_advdrastic.iDisplay_width, g_advdrastic.iDisplay_height);
    fflush(stdout);
}

void egl_terminate(void)
{
    glDeleteTextures(TEX_MAX, vid.texID);
    
#ifdef ADVDRASTIC_DRM
    eglMakeCurrent(vid.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(vid.eglDisplay, vid.eglContext);
    eglDestroySurface(vid.eglDisplay, vid.eglSurface);
    eglTerminate(vid.eglDisplay);
    
    // GBM
    gbm_surface_destroy(drm_buf.gbm_surface);
    gbm_device_destroy(drm_buf.gbm);
    close(dri_fd);
#else
    // Clean up SDL GL context
    if (sdl_gl_context != NULL) {
        SDL_GL_DeleteContext(sdl_gl_context);
        sdl_gl_context = NULL;
    }
#endif
}

void GFX_Init(void)
{
    struct stat st = {0};
    char buf[MAX_PATH << 1] = {0};
    //char layoutjson_path[MAX_PATH];

    // get resolution of the display
    SDL_DisplayMode dm;
    SDL_GetCurrentDisplayMode(0, &dm);

    printf("[trngaje] GFX_Init:%dx%d\n", dm.w, dm.h);
/*
    dm.w = 640;
    dm.h = 480;

    dm.w = 720;
    dm.h = 720;
*/
    // check whether current display is rotated
    if (dm.w >= dm.h) {
        g_advdrastic.iDisplay_width = dm.w;
        g_advdrastic.iDisplay_height = dm.h;
    }
    else {
        g_advdrastic.iDisplay_width = dm.h;
        g_advdrastic.iDisplay_height = dm.w;
    }

    fb_init();
#ifdef ADVDRASTIC_DRM
    drm_init();
#endif

    memset(nds.pen.path, 0, sizeof(nds.pen.path));
    if (getcwd(nds.pen.path, sizeof(nds.pen.path))) {
        strcat(nds.pen.path, "/");
        strcat(nds.pen.path, PEN_PATH);
    }

    strcpy(nds.shot.path, SHOT_PATH);
    if (stat(nds.shot.path, &st) == -1) {
        mkdir(nds.shot.path, 0755);
    }

    memset(buf, 0, sizeof(buf));
    if (getcwd(buf, sizeof(buf))) {
        strcat(buf, "/");
        strcat(buf, THEME_PATH);

        //strcat(nds.theme.path, "_640");
        sprintf(nds.theme.path, "%s/%dx%d", buf, g_advdrastic.iDisplay_width, g_advdrastic.iDisplay_height);
        sprintf(buf, "%s/layout.json", nds.theme.path);
        printf("[trngaje] nds.theme.path=%s\n", nds.theme.path);
        printf("[trngaje] buf=%s\n", buf);

        getjsonlayout_initialize(buf);
        printjsonlayout();
    }

    memset(nds.overlay.path, 0, sizeof(nds.overlay.path));
    if (getcwd(nds.overlay.path, sizeof(nds.overlay.path))) {
        strcat(nds.overlay.path, "/");
        strcat(nds.overlay.path, OVERLAY_PATH);
    }

    memset(nds.lang.path, 0, sizeof(nds.lang.path));
    if (getcwd(nds.lang.path, sizeof(nds.lang.path))) {
        strcat(nds.lang.path, "/");
        strcat(nds.lang.path, LANG_PATH);
    }

    memset(nds.menu.path, 0, sizeof(nds.menu.path));
    if (getcwd(nds.menu.path, sizeof(nds.menu.path))) {
        strcat(nds.menu.path, "/");
        strcat(nds.menu.path, MENU_PATH);
    }

    memset(nds.bios.path, 0, sizeof(nds.bios.path));
    if (getcwd(nds.bios.path, sizeof(nds.bios.path))) {
        strcat(nds.bios.path, "/");
        strcat(nds.bios.path, BIOS_PATH);

        sprintf(buf, "%s/drastic_bios_arm7.bin", nds.bios.path);
        write_file(buf, drastic_bios_arm7, sizeof(drastic_bios_arm7));

        sprintf(buf, "%s/drastic_bios_arm9.bin", nds.bios.path);
        write_file(buf, drastic_bios_arm9, sizeof(drastic_bios_arm9));

        sprintf(buf, "%s/nds_bios_arm7.bin", nds.bios.path);
        write_file(buf, nds_bios_arm7, sizeof(nds_bios_arm7));

        sprintf(buf, "%s/nds_bios_arm9.bin", nds.bios.path);
        write_file(buf, nds_bios_arm9, sizeof(nds_bios_arm9));

        sprintf(buf, "%s/nds_firmware.bin", nds.bios.path);
        write_file(buf, nds_firmware, sizeof(nds_firmware));
    }

    cvt = SDL_CreateRGBSurface(SDL_SWSURFACE, g_advdrastic.iDisplay_width, g_advdrastic.iDisplay_height, 32, 0, 0, 0, 0);

    nds.pen.sel = 0;
    nds.pen.max = get_pen_count();

    nds.theme.sel = 0;
    nds.theme.max = get_theme_count();

    nds.overlay.sel = nds.overlay.max = get_overlay_count();

    nds.menu.sel = 0;
    nds.menu.max = get_menu_count();

    nds.menu.drastic.main = SDL_CreateRGBSurface(SDL_SWSURFACE, g_advdrastic.iDisplay_width, g_advdrastic.iDisplay_height, 32, 0, 0, 0, 0);
/*
    if (nds.menu.drastic.main) {
        if (nds.menu.drastic.bg0) {
            SDL_SoftStretch(nds.menu.drastic.bg0, NULL, nds.menu.drastic.main, NULL);
        }
    }
*/
    getcwd(nds.cfg.path, sizeof(nds.cfg.path));
    strcat(nds.cfg.path, "/");
    strcat(nds.cfg.path, CFG_PATH);

    TTF_Init();

    // 1:1 ratio 기기에서 좌우 글자 겹침 때문에 아래 구문 추가함
    FONT_SIZE= (g_advdrastic.iDisplay_height * 24 / 480 <= g_advdrastic.iDisplay_width * 24 / 640) ?  g_advdrastic.iDisplay_height * 24 / 480 : g_advdrastic.iDisplay_width * 24 / 640;  
    nds.font = TTF_OpenFont(FONT_PATH, FONT_SIZE);
    if (nds.enable_752x560) {
        //TTF_SetFontStyle(nds.font, TTF_STYLE_BOLD);
    }


    //egl_init();
    
    is_running = 1;
    pthread_create(&thread, NULL, video_handler, (void *)NULL);
}

void GFX_Quit(void)
{
    void *ret = NULL;

    printf(PREFIX"Wait for video_handler exit\n");

    // acquire a lock
    pthread_mutex_lock(&lock);
    is_running = 0;
    //pthread_cond_destory(&request_update_screen_cond);    // build error
    pthread_cond_signal(&request_update_screen_cond);
    
    pthread_mutex_unlock(&lock);  
    
    pthread_join(thread, &ret);
    egl_terminate();
    
    GFX_Clear();
    printf(PREFIX"Free FB resources\n");
    fb_quit();

    if (cvt) {
        SDL_FreeSurface(cvt);
        cvt = NULL;
    }
}

void GFX_Clear(void)
{

}

int draw_pen(void *pixels, int width, int pitch)
{
    int c0 = 0;
    int c1 = 0;
    int w = 28;
    int h = 28;
    int sub = 0;
    int sw = NDS_W;
    int sh = NDS_H;
    int x0 = 0, y0 = 0;
    int x1 = 0, y1 = 0;
    int x = 0, y = 0, is_565 = 0, scale = 1;
    uint32_t r = 0, g = 0, b = 0;
    uint32_t *s = hex_pen;
    uint16_t *d_565 = (uint16_t*)pixels;
    uint32_t *d_888 = (uint32_t*)pixels;

    if ((pitch / width) == 2) {
        is_565 = 1;
    }

    //printf("[trngaje] draw_pen++: width=%d, is_565=%d\n", width, is_565);

    if (width == NDS_Wx2) {
        sw = NDS_Wx2;
        sh = NDS_Hx2;
        scale = 2;
    }


	x =  (uint32_t) (*((uint64_t *)VAR_SDL_SCREEN_CURSOR)) * scale; // high(32bit) y | low(32bit) x
	y =  (uint32_t)(*((uint64_t *)VAR_SDL_SCREEN_CURSOR) >> 32) * scale;
	//printf("[trngaje] draw_pen++: x=%d, y=%d\n", x, y);
	/*
    x = (evt.mouse.x * sw) / evt.mouse.maxx;
    y = (evt.mouse.y * sh) / evt.mouse.maxy;
*/
    if (nds.pen.img) {
        w = nds.pen.img->w;
        h = nds.pen.img->h;
        s = nds.pen.img->pixels;
    }

    switch(nds.pen.type) {
    case PEN_LT:
        printf("[trngaje]PEN_LT\n");
        break;
    case PEN_LB:
        printf("[trngaje]PEN_LB\n");
        y -= (h * scale);
        break;
    case PEN_RT:
        printf("[trngaje]PEN_RT\n");
        x -= (w * scale);
        break;
    case PEN_RB:
        x -= (w * scale);
        y -= (h * scale);
        break;
    case PEN_CP:
        x -= ((w * scale) >> 1);
        y -= ((h * scale) >> 1);
        break;
    }

#if 0
    vVertices[0] = ((((float)x) / NDS_W) - 0.5) * 2.0;
    vVertices[1] = ((((float)y) / NDS_H) - 0.5) * -2.0;

    vVertices[5] = vVertices[0];
    vVertices[6] = ((((float)(y + nds.pen.img->h)) / NDS_H) - 0.5) * -2.0;

    vVertices[10] = ((((float)(x + nds.pen.img->w)) / NDS_W) - 0.5) * 2.0;
    vVertices[11] = vVertices[6];

    vVertices[15] = vVertices[10];
    vVertices[16] = vVertices[1];

    glUniform1f(vid.alphaLoc, 1.0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vid.texID[TEX_PEN]);
    glVertexAttribPointer(vid.posLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vVertices);
    glVertexAttribPointer(vid.texLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    glDisable(GL_BLEND);
    glUniform1f(vid.alphaLoc, 0.0);
#endif

    //asm ("pld [%0, #128]"::"r" (s));
    for (c1=0; c1<h; c1++) {
        //asm ("pld [%0, #128]"::"r" (d_565));
        //asm ("pld [%0, #128]"::"r" (d_888));
        for (c0=0; c0<w; c0++) {
            x0 = x1 = (c0 * scale) + x;
            y0 = y1 = (c1 * scale) + (y - sub);
/*
            if (scale == 2) {
                x0 += 1;
                y0 += 1;
            }
*/
            if ((y0 >= 0) && (y0 < sh) && (x0 < sw) && (x0 >= 0)) {
                if (*s) {
                    if (is_565) {
                        r = (*s & 0xf80000) >> 8;
                        g = (*s & 0x00f800) >> 5;
                        b = (*s & 0x0000f8) >> 3;
                        d_565[(y1 * width) + x1] = r | g | b;
                        if (scale == 2) {
                            d_565[((y1 + 0) * width) + (x1 + 1)] = r | g | b;
                            d_565[((y1 + 1) * width) + (x1 + 0)] = r | g | b;
                            d_565[((y1 + 1) * width) + (x1 + 1)] = r | g | b;
                        }
                    }
                    else {
                        d_888[(y1 * width) + x1] = *s;
                        if (scale == 2) {
                            d_888[((y1 + 0) * width) + (x1 + 1)] = *s;
                            d_888[((y1 + 1) * width) + (x1 + 0)] = *s;
                            d_888[((y1 + 1) * width) + (x1 + 1)] = *s;
                        }
                    }
                }
            }
            s+= 1;
        }
    }
    //printf("[trngaje] draw_pen--\n");
    return 0;
}

int GFX_Copy(int id, const void *pixels, SDL_Rect srcrect, SDL_Rect dstrect, int pitch, int alpha, int rotate)
{
    int tex = (id >= 0) ? id : TEX_TMP;
    unsigned char  ucLayoutType;
    unsigned short usLayoutRotate;

    usLayoutRotate = getlayout_rotate(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);
    ucLayoutType = getlayout_type(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);

    //if ((id != -1) && (nds.dis_mode == NDS_DIS_MODE_HH0)) {
    if ((id != -1) && (usLayoutRotate == 90)) {
        //vVertices[5] = ((((float)dstrect.x) / 640.0) - 0.5) * 2.0;
        vVertices[5] = ((((float)dstrect.x) / g_advdrastic.iDisplay_width) - 0.5) * 2.0;
        //vVertices[6] = ((((float)dstrect.y) / 480.0) - 0.5) * -2.0;
        vVertices[6] = ((((float)dstrect.y) / g_advdrastic.iDisplay_height) - 0.5) * -2.0;

        vVertices[10] = vVertices[5];
        //vVertices[11] = ((((float)(dstrect.y + dstrect.w)) / 480.0) - 0.5) * -2.0;
        vVertices[11] = ((((float)(dstrect.y + dstrect.w)) / g_advdrastic.iDisplay_height) - 0.5) * -2.0;

        //vVertices[15] = ((((float)(dstrect.x + dstrect.h)) / 640.0) - 0.5) * 2.0;
        vVertices[15] = ((((float)(dstrect.x + dstrect.h)) / g_advdrastic.iDisplay_width) - 0.5) * 2.0;
        vVertices[16] = vVertices[11];

        vVertices[0] = vVertices[15];
        vVertices[1] = vVertices[6];
    }
    //else if ((id != -1) && (nds.dis_mode == NDS_DIS_MODE_HH1)) {
    else if ((id != -1) && (usLayoutRotate == 270)) {
        //vVertices[15] = ((((float)dstrect.x) / 640.0) - 0.5) * 2.0;
        vVertices[15] = ((((float)dstrect.x) / g_advdrastic.iDisplay_width) - 0.5) * 2.0;

        //vVertices[16] = ((((float)dstrect.y) / 480.0) - 0.5) * -2.0;
        vVertices[16] = ((((float)dstrect.y) / g_advdrastic.iDisplay_height) - 0.5) * -2.0;

        vVertices[0] = vVertices[15];
        //vVertices[1] = ((((float)(dstrect.y + dstrect.w)) / 480.0) - 0.5) * -2.0;
        vVertices[1] = ((((float)(dstrect.y + dstrect.w)) / g_advdrastic.iDisplay_height) - 0.5) * -2.0;

        //vVertices[5] = ((((float)(dstrect.x + dstrect.h)) / 640.0) - 0.5) * 2.0;
        vVertices[5] = ((((float)(dstrect.x + dstrect.h)) / g_advdrastic.iDisplay_width) - 0.5) * 2.0;

        vVertices[6] = vVertices[1];

        vVertices[10] = vVertices[5];
        vVertices[11] = vVertices[16];
    }
    else {
        //vVertices[0] = ((((float)dstrect.x) / 640.0) - 0.5) * 2.0;
        vVertices[0] = ((((float)dstrect.x) / g_advdrastic.iDisplay_width) - 0.5) * 2.0;
        //vVertices[1] = ((((float)dstrect.y) / 480.0) - 0.5) * -2.0;
        vVertices[1] = ((((float)dstrect.y) / g_advdrastic.iDisplay_height) - 0.5) * -2.0;

        vVertices[5] = vVertices[0];
        //vVertices[6] = ((((float)(dstrect.y + dstrect.h)) / 480.0) - 0.5) * -2.0;
        vVertices[6] = ((((float)(dstrect.y + dstrect.h)) / g_advdrastic.iDisplay_height) - 0.5) * -2.0;

        //vVertices[10] = ((((float)(dstrect.x + dstrect.w)) / 640.0) - 0.5) * 2.0;
        vVertices[10] = ((((float)(dstrect.x + dstrect.w)) / g_advdrastic.iDisplay_width) - 0.5) * 2.0;

        vVertices[11] = vVertices[6];

        vVertices[15] = vVertices[10];
        vVertices[16] = vVertices[1];
    }

    if (tex == TEX_TMP) {
        glBindTexture(GL_TEXTURE_2D, vid.texID[tex]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (pixel_filter) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, srcrect.w, srcrect.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    //if (((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1)) && (tex == TEX_SCR0)) {
    if ((ucLayoutType == LAYOUT_TYPE_TRANSPARENT) && (tex == TEX_SCR0)) {
        glUniform1f(vid.alphaLoc, 1.0 - ((float)nds.alpha.val / 10.0));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vid.texID[tex]);
    glVertexAttribPointer(vid.posLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vVertices);
    glVertexAttribPointer(vid.texLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

    //if (((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1)) && (tex == TEX_SCR0)) {
    if ((ucLayoutType == LAYOUT_TYPE_TRANSPARENT)  && (tex == TEX_SCR0)) {
        glUniform1f(vid.alphaLoc, 0.0);
        glDisable(GL_BLEND);
    }

    return 0;
}

void GFX_Flip(void)
{
#ifdef ADVDRASTIC_DRM
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(drm_buf.gbm_surface);
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t pitch = gbm_bo_get_stride(bo);
    uint32_t fb;

    eglSwapBuffers(vid.eglDisplay, vid.eglSurface);

    // GBM Buffer Swap
    bo = gbm_surface_lock_front_buffer(drm_buf.gbm_surface);
    handle = gbm_bo_get_handle(bo).u32;
    pitch = gbm_bo_get_stride(bo);

    drmModeAddFB(dri_fd, drm_buf.width, drm_buf.height,
                 24, 32, pitch, handle, &fb);

    drmModeSetCrtc(dri_fd, crtc_id, fb,
			0, 0, &conn_id, 1, &conn->modes[0]);

    if (drm_buf.previous_bo) {
        drmModeRmFB(dri_fd, drm_buf.fb_id);
        gbm_surface_release_buffer(drm_buf.gbm_surface, drm_buf.previous_bo);
    }

    drm_buf.fb_id = fb;
    drm_buf.previous_bo = bo;
#else
    // Use SDL GL swap for Wayland/X11
    if (found_window != NULL) {
        SDL_GL_SwapWindow(found_window);
    } else if (vid.window != NULL) {
        SDL_GL_SwapWindow(vid.window);
    }
#endif

    if (nds.theme.img) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, vid.texID[TEX_BG]);
        glVertexAttribPointer(vid.posLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), bgVertices);
        glVertexAttribPointer(vid.texLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &bgVertices[3]);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    }
}

int get_font_width(const char *info)
{
    int w = 0, h = 0;

    if (nds.font && info) {
        TTF_SizeUTF8(nds.font, info, &w, &h);
    }
    return w;
}

int get_font_height(const char *info)
{
    int w = 0, h = 0;

    if (nds.font && info) {
        TTF_SizeUTF8(nds.font, info, &w, &h);
    }
    return h;
}

const char *to_lang(const char *p)
{
    const char *info = p;
    char buf[MAX_PATH] = {0};
    int cc = 0, r = 0, len = 0;
    
    if (!strcmp(nds.lang.trans[DEF_LANG_SLOT], DEF_LANG_LANG) || (p == NULL)) {
        return p;
    }

    strcpy(buf, p);
    strcat(buf, "=");
    len = strlen(buf);
    if ((len == 0) || (len >= MAX_PATH)) {
        return 0;
    }

    for (cc=0; translate[cc]; cc++) {
        if (memcmp(buf, translate[cc], len) == 0) {
            r = 1;
            info = &translate[cc][len];
            //printf(PREFIX"Translate \'%s\' as \'%s\'\n", p, info);
            break;
        }
    }

    if (r == 0) {
        //printf(PREFIX"Failed to find the translation: \'%s\'(len=%d)\n", p, len);
        info = p;
    }
    return info;
}

int draw_info(SDL_Surface *dst, const char *info, int x, int y, uint32_t fgcolor, uint32_t bgcolor)
{
    int w = 0, h = 0;
    SDL_Color fg = {0};
    SDL_Rect rt = {0, 0, 0, 0};
    SDL_Surface *t0 = NULL;
    SDL_Surface *t1 = NULL;
    SDL_Surface *t2 = NULL;

    h = strlen(info);
    if ((nds.font == NULL) || (h == 0) || (h >= MAX_PATH)) {
        return -1;
    }

    fg.r = (fgcolor >> 16) & 0xff;
    fg.g = (fgcolor >> 8) & 0xff;
    fg.b = (fgcolor >> 0) & 0xff;
    TTF_SizeUTF8(nds.font, info, &w, &h);
    t0 = TTF_RenderUTF8_Solid(nds.font, info, fg);
    if (t0) {
        if (dst == NULL) {
            t1 = SDL_CreateRGBSurface(SDL_SWSURFACE, t0->w, t0->h, 32, 0, 0, 0, 0);
            if (t1) {
                SDL_FillRect(t1, &t1->clip_rect, bgcolor);
                SDL_BlitSurface(t0, NULL, t1, NULL);

                t2 = SDL_ConvertSurface(t1, cvt->format, 0);
                if (t2) {
                    rt.x = x;
                    rt.y = y;
                    rt.w = t2->w;
                    rt.h = t2->h;
                    GFX_Copy(-1, t2->pixels, t2->clip_rect, rt, t2->pitch, 0, E_MI_GFX_ROTATE_180);
                    SDL_FreeSurface(t2);
                }
                SDL_FreeSurface(t1);
            }
        }
        else {
            rt.x = x;
            rt.y = y;
            rt.w = w;
            rt.h = h; 

            SDL_FillRect(dst, &rt, bgcolor); 
            SDL_BlitSurface(t0, NULL, dst, &rt);
        }
        SDL_FreeSurface(t0);
    }
    return 0;
}

int reload_pen(void)
{
    static int pre_sel = -1;

    char buf[MAX_PATH] = {0};
    SDL_Surface *t = NULL;

    if (pre_sel != nds.pen.sel) {
        pre_sel = nds.pen.sel;

        if (nds.pen.img) {
            SDL_FreeSurface(nds.pen.img);
            nds.pen.img = NULL;
        }

        nds.pen.type = PEN_LB;
        if (get_file_path(nds.pen.path, nds.pen.sel, buf, 1) == 0) {
            t = IMG_Load(buf);
            if (t) {
                int x = 0;
                int y = 0;
                uint32_t *p = malloc(t->pitch * t->h);
                uint32_t *src = t->pixels;
                uint32_t *dst = p;

                for (y = 0; y < t->h; y++) {
                    for (x = 0; x < t->w; x++) {
                        *dst++ = *src++;
                    }
                }
                glBindTexture(GL_TEXTURE_2D, vid.texID[TEX_PEN]);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
                free(p);

                nds.pen.img = SDL_ConvertSurface(t, cvt->format, 0);
                SDL_FreeSurface(t);

                if (strstr(buf, "_lt")) {
                    nds.pen.type = PEN_LT;
                }
                else if (strstr(buf, "_rt")) {
                    nds.pen.type = PEN_RT;
                }
                else if (strstr(buf, "_rb")) {
                    nds.pen.type = PEN_RB;
                }
                else if (strstr(buf, "_lb")) {
                    nds.pen.type = PEN_LB;
                }
                else {
                    nds.pen.type = PEN_CP;
                }
            }
            else {
                printf(PREFIX"Failed to load pen (%s)\n", buf);
            }
        }
    }
    return 0;
}

int reload_menu(void)
{
    SDL_Surface *t = NULL;
    char folder[MAX_PATH] = {0};
    char buf[MAX_PATH << 1] = {0};

    if (get_dir_path(nds.menu.path, nds.menu.sel, folder) != 0) {
        return -1;
    }

/*
    sprintf(buf, "%s/%s", folder, MENU_BG_FILE);
    t = IMG_Load(buf);
    if (t) {
        if (nds.menu.bg) {
            SDL_FreeSurface(nds.menu.bg);
        }
        nds.menu.bg = SDL_ConvertSurface(t, cvt->format, 0);
        SDL_FreeSurface(t);
    }
*/

    sprintf(buf, "%s/%s", folder, MENU_CURSOR_FILE);
    nds.menu.cursor = IMG_Load(buf);

/*
    sprintf(buf, "%s/%s", folder, DRASTIC_MENU_BG0_FILE);
    t = IMG_Load(buf);
    if (t) {
        if (nds.menu.drastic.bg0) {
            SDL_FreeSurface(nds.menu.drastic.bg0);
        }
        nds.menu.drastic.bg0 = SDL_ConvertSurface(t, cvt->format, 0);
        SDL_FreeSurface(t);
    }

    sprintf(buf, "%s/%s", folder, DRASTIC_MENU_BG1_FILE);
    t = IMG_Load(buf);
    if (t) {
        if (nds.menu.drastic.bg1) {
            SDL_FreeSurface(nds.menu.drastic.bg1);
        }
        nds.menu.drastic.bg1 = SDL_ConvertSurface(t, cvt->format, 0);
        SDL_FreeSurface(t);
    }
*/
    sprintf(buf, "%s/%s", folder, DRASTIC_MENU_CURSOR_FILE);
    nds.menu.drastic.cursor = IMG_Load(buf);

    sprintf(buf, "%s/%s", folder, DRASTIC_MENU_YES_FILE);
    t = IMG_Load(buf);
    if (t) {
        SDL_Rect nrt = {0, 0, LINE_H - 2, LINE_H - 2};

        if (nds.menu.drastic.yes) {
            SDL_FreeSurface(nds.menu.drastic.yes);
        }
        nds.menu.drastic.yes = SDL_CreateRGBSurface(SDL_SWSURFACE, nrt.w, nrt.h, 32, t->format->Rmask, t->format->Gmask, t->format->Bmask, t->format->Amask);
        if (nds.menu.drastic.yes) {
            SDL_SoftStretch(t, NULL, nds.menu.drastic.yes, NULL);
        }
        SDL_FreeSurface(t);
    }

    sprintf(buf, "%s/%s", folder, DRASTIC_MENU_NO_FILE);
    t = IMG_Load(buf);
    if (t) {
        SDL_Rect nrt = {0, 0, LINE_H - 2, LINE_H - 2};

        if (nds.menu.drastic.no) {
            SDL_FreeSurface(nds.menu.drastic.no);
        }
        nds.menu.drastic.no = SDL_CreateRGBSurface(SDL_SWSURFACE, nrt.w, nrt.h, 32, t->format->Rmask, t->format->Gmask, t->format->Bmask, t->format->Amask);
        if (nds.menu.drastic.no) {
            SDL_SoftStretch(t, NULL, nds.menu.drastic.no, NULL);
        }
        SDL_FreeSurface(t);
    }

    return 0;
}

int reload_bg(void)
{
    static int pre_sel = -1;
    static int pre_mode = -1;

    char buf[MAX_PATH] = {0};

    SDL_Surface *t = NULL;
    SDL_Rect srt = {0, 0, g_advdrastic.iDisplay_width, g_advdrastic.iDisplay_height};
    SDL_Rect drt = {0, 0, g_advdrastic.iDisplay_width, g_advdrastic.iDisplay_height};

    if (nds.overlay.sel >= nds.overlay.max) {
        if ((pre_sel != nds.theme.sel) || (pre_mode != g_advdrastic.ucLayoutIndex[nds.hres_mode])) {
            pre_mode = g_advdrastic.ucLayoutIndex[nds.hres_mode];
            pre_sel = nds.theme.sel;

            if (nds.theme.img) {
                SDL_FreeSurface(nds.theme.img);
                nds.theme.img = NULL;
            }

            nds.theme.img = SDL_CreateRGBSurface(SDL_SWSURFACE, srt.w, srt.h, 32, 0, 0, 0, 0);
            if (nds.theme.img) {

                if (get_dir_path(nds.theme.path, nds.theme.sel, buf) == 0) {
                    int i;

                    i = g_advdrastic.ucLayoutIndex[nds.hres_mode];
                    printf("[trngaje] nds.dis_mode=%d\n", i);
                    //printf("[trngaje] p =%p\n", g_theme.layout[i].bg);
                    //printf("[trngaje] g_theme.layout[i].bg =%s\n", g_theme.layout[i].bg);
                    // printf("[%d] bg=%s\n", i, g_theme.layout[i].bg);
                    
                    // 변수를 직점 참조를 하면 오류가 있어서 함수를 통해서 접근하도록 수정함 (10/20)
                    if (getlayout_bg(nds.hres_mode, i)!= NULL) {
                        if (strlen(getlayout_bg(nds.hres_mode, i)) > 0) {
                            printf("[%d] bg=%s\n", i, getlayout_bg(nds.hres_mode, i));
                            //printf("[trngaje] g_theme.layout[nds.dis_mode].bg=%s\n", g_theme.layout[i].bg);
                            strcat(buf, "/");
                            strcat(buf, getlayout_bg(nds.hres_mode, i));

                            t = IMG_Load(buf);
                            if (t) {
                                SDL_BlitSurface(t, NULL, nds.theme.img, NULL);
                                SDL_FreeSurface(t);
                                glBindTexture(GL_TEXTURE_2D, vid.texID[TEX_BG]);
                                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, nds.theme.img->w, nds.theme.img->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nds.theme.img->pixels);
                                return 0;
                            }
                            else {
                                printf(PREFIX"Failed to load wallpaper (%s)\n", buf);
                            }
                        }
                    }
                    
                    // if bg is not exist
                    SDL_FillRect(nds.theme.img, &nds.theme.img->clip_rect, SDL_MapRGB(nds.theme.img->format, 0x00, 0x00, 0x00));
                    glBindTexture(GL_TEXTURE_2D, vid.texID[TEX_BG]);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, nds.theme.img->w, nds.theme.img->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nds.theme.img->pixels);
        
                    return 0;
                }
            }
        }
        else {
            if (nds.theme.img) {
                glBindTexture(GL_TEXTURE_2D, vid.texID[TEX_BG]);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, nds.theme.img->w, nds.theme.img->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nds.theme.img->pixels);
            }
        }
    }
    else {
        t = SDL_CreateRGBSurface(SDL_SWSURFACE, g_advdrastic.iDisplay_width, g_advdrastic.iDisplay_height, 32, 0, 0, 0, 0);
        if (t) {
            SDL_FillRect(t, &t->clip_rect, SDL_MapRGB(t->format, 0x00, 0x00, 0x00));
            GFX_Copy(-1, t->pixels, t->clip_rect, drt, t->pitch, 0, E_MI_GFX_ROTATE_180);
            SDL_FreeSurface(t);
        }
    }

    return 0;
}

int reload_overlay(void)
{
    static int pre_sel = -1;

    char buf[MAX_PATH] = {0};
    SDL_Surface *t = NULL;

    printf("[trngaje] reload_overlay++:sel=%d(max=%d), pre=%d\n", nds.overlay.sel , nds.overlay.max, pre_sel);
    if ((nds.overlay.sel < nds.overlay.max) && (pre_sel != nds.overlay.sel)) {
        pre_sel = nds.overlay.sel;
        
        printf("[trngaje] reload_overlay++:step1\n");
        if (nds.overlay.img) {
            SDL_FreeSurface(nds.overlay.img);
            nds.overlay.img = NULL;
        }

        nds.overlay.img = SDL_CreateRGBSurface(SDL_SWSURFACE, g_advdrastic.iDisplay_width, g_advdrastic.iDisplay_height, 32, 0, 0, 0, 0);
        if (nds.overlay.img) {
            printf("[trngaje] reload_overlay++:step2\n");
            SDL_FillRect(nds.overlay.img, &nds.overlay.img->clip_rect, SDL_MapRGB(nds.overlay.img->format, 0x00, 0x00, 0x00));

            if (get_file_path(nds.overlay.path, nds.overlay.sel, buf, 1) == 0) {
                printf("[trngaje] reload_overlay++:step3\n");
                t = IMG_Load(buf);
                if (t) {
                    printf("[trngaje] reload_overlay++:step4\n");
                    SDL_BlitSurface(t, NULL, nds.overlay.img, NULL);
                    SDL_FreeSurface(t);
                }
                else {
                    printf(PREFIX"Failed to load overlay (%s)\n", buf);
                }
            }
        }
    }
    
     printf("[trngaje] reload_overlay--\n");
    return 0;
}

static int MMIYOO_Available(void)
{
    const char *envr = SDL_getenv("SDL_VIDEODRIVER");
    if((envr) && (SDL_strcmp(envr, MMIYOO_DRIVER_NAME) == 0)) {
        return 1;
    }
    return 0;
}

static void MMIYOO_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

int MMIYOO_CreateWindow(_THIS, SDL_Window *window)
{
    vid.window = window;
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);
    printf(PREFIX"Window:%p, Width:%d, Height:%d\n", window, window->w, window->h);
    return 0;
}

int MMIYOO_CreateWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    return SDL_Unsupported();
}

int drastic_VideoInit(void)
{
    FILE *fd = NULL;
    char buf[MAX_PATH] = {0};

    SDL_DisplayMode mode = {0};
    SDL_VideoDisplay display = {0};


    signal(SIGTERM, sigterm_handler);


    //FB_SIZE = g_advdrastic.iDisplay_width * g_advdrastic.iDisplay_height * FB_BPP * 2;
    //TMP_SIZE = g_advdrastic.iDisplay_width * g_advdrastic.iDisplay_height * FB_BPP;

    GFX_Init();

    //LINE_H = 30;
    LINE_H = g_advdrastic.iDisplay_height *30 / 480;
    //FONT_SIZE = DEF_FONT_SIZE;
    //FONT_SIZE= g_advdrastic.iDisplay_height * 24 / 480;

    //g_advdrastic.iDisplay_width = g_advdrastic.iDisplay_width;
    //g_advdrastic.iDisplay_height = g_advdrastic.iDisplay_height;
    read_config();
    //MMIYOO_EventInit();

    detour_init(4096, nds.states.path);
    printf(PREFIX"Installed hooking for drastic functions\n");
    detour_hook(FUN_PRINT_STRING, (intptr_t)sdl_print_string);
    //detour_hook(FUN_SAVESTATE_PRE, (intptr_t)sdl_savestate_pre);
    //detour_hook(FUN_SAVESTATE_POST, (intptr_t)sdl_savestate_post);
    //detour_hook(FUN_BLIT_SCREEN_MENU, (intptr_t)sdl_blit_screen_menu);
    detour_hook(FUN_UPDATE_SCREENS, (intptr_t)sdl_update_screens);

    detour_hook(FUN_UPDATE_SCREEN, (intptr_t)sdl_update_screen);
    //detour_hook(FUN_UPDATE_FRAME, (intptr_t)sdl_update_screen);
    //detour_hook(FUN_RENDER_POLYGON_SETUP_PERSPECTIVE_STEPS, (intptr_t)render_polygon_setup_perspective_steps);    

    //detour_hook(FUN_MENU, (intptr_t)sdl_menu); // when press menu key
    //detour_hook(FUN_SET_SCREEN_ORIENTATION, (intptr_t)sdl_setscreen_orientation);
    detour_hook(FUN_UPDATE_INPUT, (intptr_t)sdl_update_input);
    detour_hook(FUN_UPDATE_SCREEN_MENU, (intptr_t)sdl_update_screen_menu);
    //detour_hook(FUN_GET_GUI_INPUT, (intptr_t)sdl_get_gui_input);
    detour_hook(FUN_DRAW_MENU_BG, (intptr_t)sdl_draw_menu_bg);
    detour_hook(FUN_QUIT, (intptr_t)sdl_quit);
    detour_hook(FUN_PLATFORM_GET_INPUT, (intptr_t)sdl_platform_get_input);
    detour_hook(FUN_SCREEN_SET_CURSOR_POSITION, (intptr_t)sdl_screen_set_cursor_position);
	detour_hook(FUN_LOAD_CONFIG_FILE, (intptr_t)sdl_load_config_file);
    detour_hook(FUN_CREATE_MENU_MAIN, (intptr_t)sdl_create_menu_main);
    detour_hook(FUN_CREATE_MENU_EXTRA_CONTROLS, (intptr_t)sdl_create_menu_extra_controls);
    detour_hook(FUN_CREATE_MENU_OPTION, (intptr_t)sdl_create_menu_options);
    detour_hook(FUN_SDL_DRAW_BUTTON_CONFIG, (intptr_t)sdl_draw_button_config);
    detour_hook(FUN_ACTION_BUTTON_CONFIG, (intptr_t)sdl_action_button_config);
    detour_hook(FUN_CONFIG_UPDATE_SETTINGS, (intptr_t)sdl_config_update_settings);
    detour_hook(FUN_SAVE_CONFIG_FILE, (intptr_t)sdl_save_config_file);
    detour_hook(FUN_SELECT_LOAD_GAME, (intptr_t)sdl_select_load_game);
    detour_hook(FUN_DRAW_NUMERIC_LABELED, (intptr_t)sdl_draw_numeric_labeled);
    detour_hook(FUN_PLATFORM_GET_CONFIG_INPUT, (intptr_t)sdl_platform_get_config_input);

    detour_hook(FUN_DRAW_MENU_OPTION, (intptr_t)sdl_draw_menu_option);
    detour_hook(FUN_SET_SCREEN_MENU_ON, (intptr_t)sdl_set_screen_menu_on);
    //detour_hook(FUN_DRAW_MENU_MAIN, (intptr_t)sdl_draw_menu_main);
    //detour_hook(FUN_SPU_LOAD_FAKE_MICROPHONE_DATA, (intptr_t)sdl_spu_load_fake_microphone_data);
#if 0
    printf(PREFIX"Installed hooking for libc functions\n");
    detour_hook(FUN_MALLOC, (intptr_t)sdl_malloc);
    detour_hook(FUN_REALLOC, (intptr_t)sdl_realloc);
    detour_hook(FUN_FREE, (intptr_t)sdl_free);
#endif
    return 0;
}

static int MMIYOO_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

void drastic_VideoQuit(void)
{
    printf(PREFIX"MMIYOO_VideoQuit\n");
    printf(PREFIX"Wait for savestate complete\n");
    /*
    while (savestate_busy) {
        usleep(1000000);
    }
    */
    system("sync");
    detour_quit();
    write_config();

    if (fps_info) {
        SDL_FreeSurface(fps_info);
        fps_info = NULL;
    }

    if (nds.pen.img) {
        SDL_FreeSurface(nds.pen.img);
        nds.pen.img = NULL;
    }

    if (nds.theme.img) {
        SDL_FreeSurface(nds.theme.img);
        nds.theme.img = NULL;
    }

    if (nds.overlay.img) {
        SDL_FreeSurface(nds.overlay.img);
        nds.overlay.img = NULL;
    }

/*
    if (nds.menu.bg) {
        SDL_FreeSurface(nds.menu.bg);
        nds.menu.bg = NULL;
    }
*/
    if (nds.menu.cursor) {
        SDL_FreeSurface(nds.menu.cursor);
        nds.menu.cursor = NULL;
    }
/*
    if (nds.menu.drastic.bg0) {
        SDL_FreeSurface(nds.menu.drastic.bg0);
        nds.menu.drastic.bg0 = NULL;
    }

    if (nds.menu.drastic.bg1) {
        SDL_FreeSurface(nds.menu.drastic.bg1);
        nds.menu.drastic.bg1 = NULL;
    }
*/
    if (nds.menu.drastic.cursor) {
        SDL_FreeSurface(nds.menu.drastic.cursor);
        nds.menu.drastic.cursor = NULL;
    }

    if (nds.menu.drastic.main) {
        SDL_FreeSurface(nds.menu.drastic.main);
        nds.menu.drastic.main = NULL;
    }

    if (nds.menu.drastic.yes) {
        SDL_FreeSurface(nds.menu.drastic.yes);
        nds.menu.drastic.yes = NULL;
    }

    if (nds.menu.drastic.no) {
        SDL_FreeSurface(nds.menu.drastic.no);
        nds.menu.drastic.no = NULL;
    }

    if (nds.font) {
        TTF_CloseFont(nds.font);
        nds.font = NULL;
    }
    TTF_Quit();
    GFX_Quit();
    //MMIYOO_EventDeinit();
    lang_unload();
}

static const char *POS[] = {
    "Top-Right", "Top-Left", "Bottom-Left", "Bottom-Right"
};

static const char *BORDER[] = {
    "None", "White", "Red", "Green", "Blue", "Black", "Yellow", "Cyan"
};

static int lang_next(void)
{
    int cc = 0;

    for (cc=1; cc<(MAX_LANG_FILE-1); cc++) {
        if (!strcmp(nds.lang.trans[DEF_LANG_SLOT], nds.lang.trans[cc])) {
            if (strcmp(nds.lang.trans[cc + 1], "")) {
                strcpy(nds.lang.trans[DEF_LANG_SLOT], nds.lang.trans[cc + 1]);
                return 0;
            }
        }
    }
    return -1;
}

static int lang_prev(void)
{
    int cc = 0;

    for (cc=(MAX_LANG_FILE-1); cc>1; cc--) {
        if (!strcmp(nds.lang.trans[DEF_LANG_SLOT], nds.lang.trans[cc])) {
            if (strcmp(nds.lang.trans[cc - 1], "")) {
                strcpy(nds.lang.trans[DEF_LANG_SLOT], nds.lang.trans[cc - 1]);
                return 0;
            }
        }
    }
    return -1;
}

enum {
    MENU_LANG = 0,
#if 0
    MENU_CPU,
    MENU_OVERLAY,
#endif
    MENU_DIS,
    MENU_DIS_ALPHA,
    MENU_DIS_BORDER,
    MENU_DIS_POSITION,
#if 0
    MENU_ALT,
    MENU_KEYS,
    MENU_HOTKEY,
    MENU_SWAP_L1L2,
    MENU_SWAP_R1R2,
    MENU_PEN_XV,
    MENU_PEN_YV,
#endif
    MENU_CURSOR,
#if 0
    MENU_FAST_FORWARD,
#endif
    MENU_LAST,
};

static const char *MENU_ITEM[] = {
    "Language",
#if 0
    "CPU",
    "Overlay",
#endif
    "Display",
    "Alpha",
    "Border",
    "Position",
#if 0
    "Alt. Display",

    "Keys",

    "Hotkey",
    "Swap L1-L2",
    "Swap R1-R2",
    "Pen X Speed",
    "Pen Y Speed",
#endif
    "Cursor",
#if 0
    "Fast Forward",
#endif
};


int handle_menu(int key)
{
    static int pre_ff = 0;
    static int cur_sel = 0;
    static uint32_t cur_cpuclock = 0;
    static uint32_t pre_cpuclock = 0;
    static char pre_lang[LANG_FILE_LEN] = {0};

    const int SX = g_advdrastic.iDisplay_width * 0.1;
    const int SY = g_advdrastic.iDisplay_height * 0.1;
    const int SSX = g_advdrastic.iDisplay_width * 0.5;

    int cc = 0;
    int sx = 0;
    int sy = 0;
    int s0 = 0;
    int s1 = 0;
    int idx = 0;
    int h = LINE_H;
    int dis_mode = -1;
    SDL_Rect rt = {0};
    char buf[MAX_PATH] = {0};
    uint32_t sel_col = nds.menu.c0;
    uint32_t unsel_col = nds.menu.c1;
    uint32_t dis_col = 0x808080;
    uint32_t val_col = nds.menu.c0;
    uint32_t col0 = 0, col1 = 0;

    uint32_t bgcolor;

    unsigned char  ucLayoutType;
    unsigned short  usLayoutRotate;

    usLayoutRotate = getlayout_rotate(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);
    ucLayoutType = getlayout_type(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);
    
    if (pre_lang[0] == 0) {
        strcpy(pre_lang, nds.lang.trans[DEF_LANG_SLOT]);
    }

    switch (key) {
    case DRASTIC_GUI_INPUT_UP:
        if (cur_sel > 0) {
            cur_sel-= 1;
        }
        break;
    case DRASTIC_GUI_INPUT_DOWN:
        if (cur_sel < (MENU_LAST - 1)) {
            cur_sel+= 1;
        }
        break;
    case DRASTIC_GUI_INPUT_LEFT:
        switch(cur_sel) {
        case MENU_LANG:
            lang_prev();
            break;

        case MENU_DIS:
            if (g_advdrastic.ucLayoutIndex[nds.hres_mode] > 0) {
                g_advdrastic.ucLayoutIndex[nds.hres_mode] -= 1;
            }

            break;
        case MENU_DIS_ALPHA:
            //if (((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1))) {
            if ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT) {
                if (nds.alpha.val > 0) {
                    nds.alpha.val-= 1;
                }
            }
            break;
        case MENU_DIS_BORDER:
            if ((nds.alpha.val > 0) && ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT)/*((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1))*/) {
                if (nds.alpha.border > 0) {
                    nds.alpha.border-= 1;
                }
            }
            break;
        case MENU_DIS_POSITION:
            //if (((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1))) {
            if ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT) {
                if (nds.alpha.pos > 0) {
                    nds.alpha.pos-= 1;
                }
            }
            break;

        case MENU_CURSOR:
            nds.menu.show_cursor = 0;
            break;

        default:
            break;
        }
        break;
    case DRASTIC_GUI_INPUT_RIGHT:
        switch(cur_sel) {
        case MENU_LANG:
            lang_next();
            break;
        case MENU_DIS:
            if (g_advdrastic.ucLayoutIndex[nds.hres_mode] < (getmax_layout(nds.hres_mode)-1)) {
                g_advdrastic.ucLayoutIndex[nds.hres_mode] += 1;
            }

            break;
        case MENU_DIS_ALPHA:
            //if (((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1))) {
            if ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT) {
                if (nds.alpha.val < NDS_ALPHA_MAX) {
                    nds.alpha.val+= 1;
                }
            }
            break;
        case MENU_DIS_BORDER:
            if ((nds.alpha.val > 0) && ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT)/*((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1))*/) {
                if (nds.alpha.border < NDS_BORDER_MAX) {
                    nds.alpha.border+= 1;
                }
            }
            break;
        case MENU_DIS_POSITION:
            //if (((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1))) {
            if ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT) {
                if (nds.alpha.pos < 3) {
                    nds.alpha.pos+= 1;
                }
            }
            break;
        case MENU_CURSOR:
            nds.menu.show_cursor = 1;
            break;

        default:
            break;
        }
        break;
    case DRASTIC_GUI_INPUT_B:
        if (cur_cpuclock != pre_cpuclock) {
            pre_cpuclock = cur_cpuclock;
        }

        if (strcmp(pre_lang, nds.lang.trans[DEF_LANG_SLOT])) {
            lang_unload();
            lang_load(nds.lang.trans[DEF_LANG_SLOT]);
            memset(pre_lang, 0, sizeof(pre_lang));
        }
/*
        if (pre_ff != nds.fast_forward) {
            //dtr_fastforward(nds.fast_forward);
            pre_ff = nds.fast_forward;
        }
*/
        nds.menu.enable = 0;

        return 0;
    default:
        break;
    }

    if (cur_sel == MENU_DIS) {
        dis_mode = g_advdrastic.ucLayoutIndex[nds.hres_mode];
    }

    //SDL_SoftStretch(nds.menu.bg, NULL, cvt, NULL);
    SDL_FillRect(cvt, NULL, SDL_MapRGB(nds.menu.drastic.main->format, 0, 0, 0));
/*
    if (cur_sel < 3) {
        s0 = 0;
        s1 = 7;
    }
    else if (cur_sel >= (MENU_LAST - 4)) {
        s0 = MENU_LAST - 8;
        s1 = MENU_LAST - 1;
    }
    else {
        s0 = cur_sel - 3;
        s1 = cur_sel + 4;
    }
*/
    if (cur_sel < 6) {
        s0 = 0;
        s1 = 13;
    }
    else if (cur_sel >= (MENU_LAST - 7)) {
        s0 = MENU_LAST - 14;
        s1 = MENU_LAST - 1;
    }
    else {
        s0 = cur_sel - 6;
        s1 = cur_sel + 7;
    }

    for (cc=0, idx=0; cc<MENU_LAST; cc++) {
        if ((cc < s0) || (cc > s1)) {
            continue;
        }

        sx = 0;
        col0 = (cur_sel == cc) ? sel_col : unsel_col;
        col1 = (cur_sel == cc) ? val_col : unsel_col;
        switch (cc) {
        case MENU_DIS_ALPHA:
            //sx = 20;
            sx = g_advdrastic.iDisplay_width * 20 / 640;
            if ((cur_sel == MENU_DIS_ALPHA) && ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT)/*((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1))*/) {
                col1 = val_col;
            }
            else {
                //if ((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1)) {
                if ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT) {
                    col1 = unsel_col;
                }
                else {
                    col1 = dis_col;
                }
            }
            break;
        case MENU_DIS_BORDER:
            //sx = 20;
            sx = g_advdrastic.iDisplay_width * 20 / 640;
            if ((cur_sel == MENU_DIS_BORDER) && (nds.alpha.val > 0) && ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT)/*((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1))*/) {
                col1 = val_col;
            }
            else {
                if ((nds.alpha.val > 0) && ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT)/*((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1))*/) {
                    col1 = unsel_col;
                }
                else {
                    col1 = dis_col;
                }
            }
            break;
        case MENU_DIS_POSITION:
            //sx = 20;
            sx = g_advdrastic.iDisplay_width * 20 / 640;
            if ((cur_sel == MENU_DIS_POSITION) && ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT)/*((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1))*/) {
                col1 = val_col;
            }
            else {
                //if ((nds.dis_mode == NDS_DIS_MODE_VH_T0) || (nds.dis_mode == NDS_DIS_MODE_VH_T1)) {
                if ( ucLayoutType == LAYOUT_TYPE_TRANSPARENT) {
                    col1 = unsel_col;
                }
                else {
                    col1 = dis_col;
                }
            }
            break;

        default:
            break;
        }

        if (col0 == sel_col) {
            if (nds.menu.show_cursor) {
                //rt.x = SX - 10;
                //rt.w = 421;
                rt.x = SX - g_advdrastic.iDisplay_width * 10 / 640;
                rt.w = g_advdrastic.iDisplay_width * 421 / 640;
            }
            else {
                rt.x = SX - g_advdrastic.iDisplay_width * 50 / 640;
                rt.w = g_advdrastic.iDisplay_width * 461/640;
            }
            rt.y = SY + (h * idx) - 2;
            rt.h = FONT_SIZE + 3;

            if ((cc == MENU_DIS_ALPHA) /*|| (cc == MENU_KEYS)*/) {
                rt.w-= g_advdrastic.iDisplay_width * 121 / 640;
            }
            if (col1 == dis_col) {
                col1 = val_col;
                //SDL_FillRect(cvt, &rt, SDL_MapRGB(nds.menu.drastic.main->format, 0x80, 0x80, 0x80));
                bgcolor = SDL_MapRGB(nds.menu.drastic.main->format, 0x80, 0x80, 0x80);
            }
            else {
                //SDL_FillRect(cvt, &rt, SDL_MapRGB(nds.menu.drastic.main->format, (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff));
                bgcolor =  SDL_MapRGB(nds.menu.drastic.main->format, (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
            }

            if (nds.menu.show_cursor && nds.menu.cursor) {
                rt.x -= (nds.menu.cursor->w + g_advdrastic.iDisplay_width * 5 / 640);
                rt.y -= ((nds.menu.cursor->h - LINE_H) / 2);
                rt.w = 0;
                rt.h = 0;
                SDL_BlitSurface(nds.menu.cursor, NULL, cvt, &rt);
            }

            if ((cc == MENU_DIS)/* || (cc == MENU_ALT)*/) {
                rt.x = g_advdrastic.iDisplay_width * 440 / 640;
                rt.y = SY + (h * (idx + 1)) - g_advdrastic.iDisplay_height * 7 / 480;
                rt.w = g_advdrastic.iDisplay_width * 121 / 640;
                rt.h = FONT_SIZE + g_advdrastic.iDisplay_height * 8 / 480;
                //SDL_FillRect(cvt, &rt, SDL_MapRGB(nds.menu.drastic.main->format, (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff));
               //bgcolor =  SDL_MapRGB(nds.menu.drastic.main->format, (nds.menu.c2 >> 16) & 0xff, (nds.menu.c2 >> 8) & 0xff, nds.menu.c2 & 0xff);
            }
        }
        draw_info(cvt, to_lang(MENU_ITEM[cc]), SX + sx, SY + (h * idx), col0, 0);

        sx = 0;
        switch (cc) {
        case MENU_LANG:
            sprintf(buf, "%s", nds.lang.trans[DEF_LANG_SLOT]);
            break;

        case MENU_DIS:
            sprintf(buf, "[%d]", g_advdrastic.ucLayoutIndex[nds.hres_mode]);
/*
            if (nds.hres_mode == 0) {
                sprintf(buf, "[%d]   %s", nds.dis_mode, nds.enable_752x560 ? DIS_MODE0_752[nds.dis_mode] : DIS_MODE0_640[nds.dis_mode]);
            }
            else {
                sprintf(buf, "[%d]   %s", nds.dis_mode, nds.dis_mode == NDS_DIS_MODE_HRES0 ? "512*384" : "640*480");
            }
*/
            break;
        case MENU_DIS_ALPHA:
            sx = 0;
            sprintf(buf, "%d", nds.alpha.val);
            break;
        case MENU_DIS_BORDER:
            sprintf(buf, "%s", to_lang(BORDER[nds.alpha.border]));
            break;
        case MENU_DIS_POSITION:
            sprintf(buf, "%s", to_lang(POS[nds.alpha.pos]));
            break;

        case MENU_CURSOR:
            sprintf(buf, "%s", to_lang(nds.menu.show_cursor ? "Show" : "Hide"));
            break;

        }
        draw_info(cvt, buf, SSX + sx, SY + (h * idx), col1, 0);
        idx+= 1;
    }

    sx = g_advdrastic.iDisplay_width * 450 / 640;
    sy = g_advdrastic.iDisplay_height * 360 / 480;

    process_displaylayout(g_advdrastic.iDisplay_width * 0.1, g_advdrastic.iDisplay_height  * 0.5, g_advdrastic.iDisplay_width * 0.3, g_advdrastic.iDisplay_height * 0.3);

    nds.update_menu = 1;

    need_reload_bg = RELOAD_BG_COUNT;
    return 0;
}

/*
[trngaje] sdl_select_load_game:auStack_408=1062 - Picross DS (E)(Legacy).nds
[trngaje] sdl_select_load_game:iVar2=0
Attempting to load lua script /userdata/system/drastic/scripts/1062 - Picross DS (E)(Legacy).lua
Attempting to load lua script /userdata/system/drastic/scripts/default.lua
[trngaje] sdl_select_load_game:iVar2=0

*/
void sdl_select_load_game(long *param_1)
{
    undefined4 uVar1;
    int iVar2;
    long lVar3;
    undefined auStack_408[1024];

    nds_load_file   load_file;
    nds_load_nds    load_nds;
    
    load_file = (nds_load_file)FUN_LOAD_FILE;
    load_nds = (nds_load_nds)FUN_LOAD_NDS;

    printf("[trngaje] sdl_select_load_game++\n");
    // void load_file(long *param_1,char **param_2,char *param_3)
    // nds_ext : 0015b3e0
    iVar2 = load_file(param_1,base_addr_rx + 0x0015b3e0/*nds_ext*/, auStack_408);
    printf("[trngaje] sdl_select_load_game:auStack_408=%s\n", auStack_408);
    if (iVar2 != -1) {
        printf("[trngaje] sdl_select_load_game:iVar2=%d\n", iVar2);
        lVar3 = *param_1;
        iVar2 = load_nds(lVar3 + 800,auStack_408);
        if (-1 < iVar2) {
            printf("[trngaje] sdl_select_load_game:iVar2=%d\n", iVar2);
            // 새로운 롬이 실행이 되면 여기 까지 진입함
            uVar1 = *(undefined4 *)(lVar3 + 0x859b4);
            *(undefined8 *)((long)param_1 + 0x44) = 0x100000001;
            *(undefined4 *)((long)param_1 + 0x4c) = 0;
            *(undefined4 *)(param_1 + 10) = uVar1;
            nds.menu.drastic.enable = 0;
        }
    }

    return;
}

void sdl_menu(long param_1,int param_2)
{
    uint *puVar1;
    long lVar2;
    int iVar3;
    undefined8 *puVar4;
    uint uVar5;
    undefined4 uVar6;
    int iVar7;
    undefined8 *__ptr;
    bool bVar8;
    long lVar9;
    
    typedef void  (*nds_func3_cb_t)(long *, long, int);
    typedef int (*nds_func3r_cb_t)(long *, long, long);
    typedef int (*nds_func3r_2nd_cb_t)(long, long, int *);
    //typedef void  (*nds_func2_cb_t)(unsigned long *, unsigned long *);
    typedef void  (*nds_func2_cb_t)(unsigned long, unsigned long);
    typedef void  (*nds_func0_cb_t)();
    //code *pcVar10;
    nds_func3_cb_t *pcVar10;
    
    ulong uVar11;
    undefined8 *puVar12;
    void *pvVar13;
    int local_580 [2];
    long local_578;
    uint *local_570;
    undefined8 *local_568;
    undefined8 uStack_560;
    undefined8 local_558;
    void *local_550;
    void *local_548;
    void *local_540;
    int local_538;
    int local_534;
    int iStack_530;
    undefined4 uStack_52c;
    uint local_528;
    uint local_524;
    //undefined8 local_520;
    unsigned char local_520[8];

    
    byte local_518;
    byte local_517;
    byte local_516;
    undefined local_515 [261];
    int local_410;
    undefined auStack_408 [1024];
    //long local_8;
    unsigned char *nds_system;

    nds_screen_copy16       screen_copy16;
    nds_load_file           load_file;
    nds_load_nds            load_nds;
    nds_get_gui_input       get_gui_input;
    nds_clear_gui_actions   clear_gui_actions;
    nds_load_logo           load_logo;
    nds_create_menu_main    create_menu_main;
    nds_audio_pause         audio_pause;
    nds_set_screen_menu_on  set_screen_menu_on;
    nds_draw_menu_bg        draw_menu_bg;
    nds_set_font_narrow     set_font_narrow;
    nds_set_font_wide       set_font_wide;
    nds_update_screen_menu          update_screen_menu;
    nds_audio_revert_pause_state    audio_revert_pause_state;
    nds_config_update_settings      config_update_settings;
    nds_set_screen_menu_off         set_screen_menu_off;
    nds_reset_system                reset_system;
    nds_print_string                       print_string;
     nds_delay_us            delay_us;

    nds_action_select_menu action_select_menu; // 임시로 추가
    nds_draw_menu_option draw_menu_option;


    screen_copy16            = (nds_screen_copy16)FUN_SCREEN_COPY16;
    load_file                = (nds_load_file)FUN_LOAD_FILE;
    load_nds                 = (nds_load_nds)FUN_LOAD_NDS;
    get_gui_input            = (nds_get_gui_input)FUN_GET_GUI_INPUT;
    clear_gui_actions        = (nds_clear_gui_actions)FUN_CLEAR_GUI_ACTIONS;
    load_logo                = (nds_load_logo)FUN_LOAD_LOGO;
    create_menu_main         = (nds_create_menu_main)FUN_CREATE_MENU_MAIN;
    audio_pause              = (nds_audio_pause)FUN_AUDIO_PAUSE;
    set_screen_menu_on       = (nds_set_screen_menu_on)FUN_SET_SCREEN_MENU_ON;
    draw_menu_bg             = (nds_draw_menu_bg)FUN_DRAW_MENU_BG;
    set_font_narrow          = (nds_set_font_narrow)FUN_SET_FONT_NARROW;
    set_font_wide            = (nds_set_font_wide)FUN_SET_FONT_WIDE;
    update_screen_menu       = (nds_update_screen_menu)FUN_UPDATE_SCREEN_MENU;
    audio_revert_pause_state = (nds_audio_revert_pause_state)FUN_AUDIO_REVERT_PAUSE_STATE;
    config_update_settings   = (nds_config_update_settings)FUN_CONFIG_UPDATE_SETTINGS;
    set_screen_menu_off      = (nds_set_screen_menu_off)FUN_SET_SCREEN_MENU_OFF;
    reset_system             = (nds_reset_system)FUN_RESET_SYSTEM;
    print_string                    = (nds_print_string)FUN_PRINT_STRING;
    delay_us                        = (nds_delay_us)FUN_DELAY_US;   

    action_select_menu = (nds_action_select_menu)FUN_ACTION_SELECT_MENU; // 임시로 추가
     draw_menu_option = (nds_draw_menu_option)FUN_DRAW_MENU_OPTION;

    printf("[trngaje] sdl_menu:param_1=0x%x, param_2=0x%x\n", param_1, param_2);

    local_528 = *(uint *)(param_1 + 0x859b4);
    puVar1 = (uint *)(param_1 + 0x85568);
    iVar7 = *(int *)(param_1 + 0x859f0);
    //local_8 = ___stack_chk_guard;
    if (2 < local_528) {
        local_528 = local_528 - 1;
    }
    // 8bytes 를 합치는 루틴
    //[trngaje] sdl_menu : 0x85580:0x7400000053
    //[trngaje] sdl_menu : 0x85578:0x6100000072
    //[trngaje] sdl_menu : 0x85570:0x4400000020
    //[trngaje] sdl_menu : 0x85568:0x7200000044
    //[trngaje] sdl_menu : local_520:0x74, 0x53,0x61,0x72, 0x44,0x20,0x72,0x44

    //printf("[trngaje] sdl_menu : 0x85580:%p\n", (ulong)*(undefined8 *)(param_1 + 0x85580));
    //printf("[trngaje] sdl_menu : 0x85578:%p\n", (ulong)*(undefined8 *)(param_1 + 0x85578));
    //printf("[trngaje] sdl_menu : 0x85570:%p\n", (ulong)*(undefined8 *)(param_1 + 0x85570));
    //printf("[trngaje] sdl_menu : 0x85568:%p\n", (ulong)*(undefined8 *)(param_1 + 0x85568));
/*
    local_520 = CONCAT17((char)((ulong)*(undefined8 *)(param_1 + 0x85580) >> 0x20),
                       CONCAT16((char)*(undefined8 *)(param_1 + 0x85580),
                                CONCAT15((char)((ulong)*(undefined8 *)(param_1 + 0x85578) >> 0x20),
                                         CONCAT14((char)*(undefined8 *)(param_1 + 0x85578),
                                                  CONCAT13((char)((ulong)*(undefined8 *)
                                                                          (param_1 + 0x85570) >>
                                                                 0x20),
                                                           CONCAT12((char)*(undefined8 *)
                                                                           (param_1 + 0x85570),
                                                                    CONCAT11((char)((ulong)*(
                                                  undefined8 *)(param_1 + 0x85568) >> 0x20),
                                                  (char)*(undefined8 *)(param_1 + 0x85568))))))));
*/
    local_520[7] = (char)((ulong)*(undefined8 *)(param_1 + 0x85580) >> 0x20);
    local_520[6] = (char)*(undefined8 *)(param_1 + 0x85580);
    local_520[5] = (char)((ulong)*(undefined8 *)(param_1 + 0x85578) >> 0x20);
    local_520[4] = (char)*(undefined8 *)(param_1 + 0x85578);
    local_520[3] = (char)((ulong)*(undefined8 *)(param_1 + 0x85570) >> 0x20);
    local_520[2] = (char)*(undefined8 *)(param_1 + 0x85570);
    local_520[1] = (char)((ulong)*(undefined8 *)(param_1 + 0x85568) >> 0x20);
    local_520[0] = (char)*(undefined8 *)(param_1 + 0x85568);

    //printf("[trngaje] sdl_menu : local_520:0x%x, 0x%x,0x%x,0x%x, 0x%x,0x%x,0x%x,0x%x\n", 
    //    local_520[7], local_520[6], local_520[5], local_520[4], local_520[3], local_520[2], local_520[1], local_520[0]);

    local_534 = 0;
    local_524 = 0;
    local_518 = (byte)*(undefined4 *)(param_1 + 0x85588);
    local_517 = (byte)*(undefined4 *)(param_1 + 0x8558c);
    local_516 = (byte)*(undefined4 *)(param_1 + 0x85590);
    local_410 = 0;

    //[trngaje] sdl_menu : local_518=105, local_517=99, local_516=0
    //[trngaje] sdl_menu : iVar7=0
    
    printf("[trngaje] sdl_menu : local_518=%d, local_517=%d, local_516=%d\n",  local_518, local_517, local_516);
    printf("[trngaje] sdl_menu : iVar7=%d\n",  iVar7);
    if (iVar7 == 100000) {
        bVar8 = true;
        uVar5 = 1;
LAB_0009355c:
        if (iVar7 == 20000) {
            bVar8 = true;
            uVar5 = 4;
            goto LAB_00093a34;
        }
        if (iVar7 != 0x411a) goto LAB_000936b0;
        uVar5 = 5;
    }
    else {
        if (iVar7 == 0x8235) {
            bVar8 = true;
            uVar5 = 2;
            goto LAB_0009355c;
        }
        if (iVar7 != 25000) {
            bVar8 = false;
            uVar5 = 0;
            goto LAB_0009355c;
        }
        bVar8 = true;
        uVar5 = 3;
LAB_000936b0:
        if (iVar7 == 0x37cd) {
            uVar5 = 6;
        }
        else {
LAB_00093a34:
            if (iVar7 == 0x30d4) {
                uVar5 = 7;
            }
            else if (!bVar8) goto LAB_0009357c;
        }
    }
    local_524 = uVar5;
LAB_0009357c:
    local_578 = param_1;
    local_570 = puVar1;
    if (*(char *)(param_1 + 0x8ab38) == '\0') {
        local_538 = 0;
    }
    else {
        local_538 = 1;
        pvVar13 = malloc(0x18000);
        local_550 = pvVar13;
        local_548 = malloc(0x18000);
        screen_copy16(pvVar13,0);
        screen_copy16(local_548,1);
    }
    load_logo(&local_578);
    /*
    [trngaje] sdl_create_menu_main++
    SS: 800 480
    [trngaje] 0x0
    [SDL] sdl_print_string:p=Version r2.5.2.0, fg=0xffff, bg=0x0, x=352, y=201
    */
    __ptr = (undefined8 *)create_menu_main(&local_578);

    printf("[trngaje] sdl_menu:step1\n");
    uStack_560 = 0;
    local_558 = 0;
    iStack_530 = 0;
    uStack_52c = 1;
    local_515[0] = 0;
    local_568 = __ptr;
    printf("[trngaje] sdl_menu:step2\n");
    uVar6 = audio_pause(param_1 + 0x1586000);
    printf("[trngaje] sdl_menu:step3\n");
    set_screen_menu_on();
    printf("[trngaje] sdl_menu:step4\n");
    if (param_2 != 0) {
        // 여긴 진입 안함
        printf("[trngaje] sdl_menu:step5\n");
        // nds_ext : 0015b3e0
        iVar7 = load_file(&local_578, base_addr_rx + 0x0015b3e0/*&nds_ext*/,auStack_408);
        printf("[trngaje] sdl_menu:step6: auStack_408=%s \n", auStack_408);
        printf("[trngaje] sdl_menu:step6: local_578=%p \n", local_578);
        lVar9 = local_578;
        if ((iVar7 != -1) && (iVar7 = load_nds(local_578 + 800,auStack_408), -1 < iVar7)) {
            printf("[trngaje] sdl_menu:step7\n");
            local_528 = *(uint *)(lVar9 + 0x859b4);
            local_534 = 1;
            iStack_530 = 1;
            uStack_52c = 0;
        }
    }
    puVar12 = local_568;
    printf("[trngaje] sdl_menu:step8\n");
LAB_000935e0: // loop 
    local_568 = puVar12;
    puVar4 = local_568;
    if ((iStack_530 == 0) || (*(char *)(param_1 + 0x8ab38) == '\0')) {
        printf("[trngaje] sdl_menu:step9\n");
        //delay_us("k"); // 수정 필요함
        delay_us(100000); 
        printf("[trngaje] sdl_menu:step9-1\n");
        draw_menu_bg(&local_578);
        printf("[trngaje] sdl_menu:step10\n");
        set_font_narrow();
        //print_string(local_515,0xa676,0,0x10); //error: too few arguments to function ‘print_string’
        printf("[trngaje] sdl_menu:step11:local_515=0x%x\n", *local_515);
        //[SDL] sdl_print_string:p=, fg=0xa676, bg=0x0, x=16, y=16
        print_string(local_515,0xa676,0,0x10,0x10);
         // 비교:sdl_print_string("Version r2.5.2.0",0xffff,0,uVar2,0xc9);
        set_font_wide();
        if (*(int *)((long)puVar4 + 0x14) != 0) {
            printf("[trngaje] sdl_menu:step13:*(int *)((long)puVar4 + 0x14)=0x%x\n", *(int *)((long)puVar4 + 0x14)); // 전체 갯수 같은데..
            printf("[trngaje] sdl_menu:step13:*(int *)(puVar4 + 3)=0x%x\n", *(int *)(puVar4 + 3)); 

            uVar11 = 0; // 증가 값 초기화
            do {
                lVar9 = *(long *)(puVar4[4] + uVar11 * 8);
                iVar7 = (int)uVar11;
                uVar5 = iVar7 + 1;
                uVar11 = (ulong)uVar5;
                //(**(code **)(lVar9 + 0x10))(&local_578,lVar9,*(int *)(puVar4 + 3) == iVar7); // callback 함수 정의 필요함
                printf("[trngaje] sdl_menu:step13:lVar9 + 0x10=%p, iVar7=%d\n", lVar9 + 0x10, iVar7); 
                printf("[trngaje] sdl_menu:step13:offset of (lVar9 + 0x10)=%p\n", lVar9 + 0x10 - base_addr_rx); 
                // 아래 두개 함수는 결과가 같음
                //(*(nds_func3_cb_t *)(lVar9 + 0x10))(&local_578,lVar9,*(int *)(puVar4 + 3) == iVar7);
                draw_menu_option(&local_578,lVar9,*(int *)(puVar4 + 3) == iVar7);
                // [SDL] sdl_print_string:p=Change Options, fg=0xffff, bg=0x17, x=180, y=280 이 값 대신
                // 아래와 같이 읽혀짐. x값 이상
                // [SDL] sdl_print_string:p=Change Options, fg=0xffff, bg=0x17, x=-1041775248, y=280

/*
// 정상적인 경우 아래와 같이 x 값이 180으로 출력되어야 함
[SDL] sdl_print_string:p=Change Options, fg=0xffff, bg=0x17, x=180, y=280
[SDL] sdl_print_string:p=Change Steward Options, fg=0xffff, bg=0x0, x=180, y=288
[SDL] sdl_print_string:p=Configure Controls, fg=0xffff, bg=0x0, x=180, y=296
[SDL] sdl_print_string:p=Configure Firmware, fg=0xffff, bg=0x0, x=180, y=304
[SDL] sdl_print_string:p=Configure Cheats, fg=0xffff, bg=0x0, x=180, y=312
[SDL] sdl_print_string:p=Load state   0, fg=0xffff, bg=0x0, x=180, y=320
[SDL] sdl_print_string:p=Save state   0, fg=0xffff, bg=0x0, x=180, y=328
[SDL] sdl_print_string:p=Load new game, fg=0xffff, bg=0x0, x=180, y=344
[SDL] sdl_print_string:p=Restart game, fg=0xffff, bg=0x0, x=180, y=352
[SDL] sdl_print_string:p=Return to game, fg=0xffff, bg=0x0, x=180, y=368
[SDL] sdl_print_string:p=Exit DraStic-trngaje, fg=0xffff, bg=0x0, x=180, y=384
*/

            } while (uVar5 < *(uint *)((long)puVar4 + 0x14));
        }

        if ((nds_func2_cb_t *)(*puVar4) != (nds_func2_cb_t *)0x0) {
             printf("[trngaje] sdl_menu:step14:puVar4=%p, *puVar4=%p\n", puVar4, *puVar4);
            // [trngaje] sdl_menu:step14:offset of*puVar4=0x8e2a0 : void draw_menu_main(void)
/*
                                 *************************************************************************
                                 *                               FUNCTION                                *
                                 *************************************************************************
                                 undefined draw_menu_main()
               undefined            w0:1             <RETURN>
                                 draw_menu_main                                          XREF[3]:       Entry Point(*), 
                                                                                                         create_menu_main:00092d08(*), 
                                                                                                         create_menu_main:00092d14(*)  
           0008e2a0 c0 03 5f d6       ret
           0008e2a4 1f                ??           1Fh
           0008e2a5 20                ??           20h     
           0008e2a6 03                ??           03h
           0008e2a7 d5                ??           D5h
           0008e2a8 1f                ??           1Fh
           0008e2a9 20                ??           20h     
           0008e2aa 03                ??           03h
           0008e2ab d5                ??           D5h
           0008e2ac 1f                ??           1Fh
           0008e2ad 20                ??           20h     
           0008e2ae 03                ??           03h
           0008e2af d5                ??           D5h
*/
             printf("[trngaje] sdl_menu:step14:offset of*puVar4=%p\n", (*puVar4) - base_addr_rx);
            //(*(code *)*puVar4)(&local_578,puVar4); // callback 함수 정의 필요함




            //아래 함수 호출시 죽음
            //(*(nds_func2_cb_t *)(*puVar4))(&local_578, puVar4);
        }
        printf("[trngaje] sdl_menu:step14-3\n");
        lVar9 = *(long *)(puVar4[4] + (ulong)*(uint *)(puVar4 + 3) * 8);
        printf("[trngaje] sdl_menu:step15\n");
        update_screen_menu();
        if (local_410 == 0) {
            lVar2 = local_578 + 0x5528;
            do {
                get_gui_input(lVar2,local_580);
            } while (local_580[0] == 0xb);
        }
        printf("[trngaje] sdl_menu:step16\n");
        puVar12 = local_568;
        if (*(nds_func3_cb_t **)(lVar9 + 0x18) != (nds_func3_cb_t *)0x0) {
            printf("[trngaje] sdl_menu:step17:lVar9 + 0x18=%p\n", lVar9 + 0x18);
            printf("[trngaje] sdl_menu:step17:offset of (lVar9 + 0x18)=%p\n", (lVar9 + 0x18) - base_addr_rx);
            //iVar7 = (**(code **)(lVar9 + 0x18))(&local_578,lVar9,local_580); // callback 함수 정의 필요함
            // 여기 아래도 죽음 : int action_select_menu(long param_1,long param_2,int *param_3) 이 함수 일까?
            //iVar7 = (**(nds_func3r_2nd_cb_t **)(lVar9 + 0x18))(&local_578,lVar9,local_580);
            iVar7 = action_select_menu(&local_578,lVar9,local_580);

            puVar12 = local_568;
            printf("[trngaje] sdl_menu:step18:iVar7=%d\n",iVar7); // 0:up, 1:down, 2:left, 3:right return 됨
            if (iVar7 != DRASTIC_GUI_INPUT_DOWN) { // 1
                if (iVar7 != DRASTIC_GUI_INPUT_UP) { // 0
                    if (iVar7 == DRASTIC_GUI_INPUT_B) { // 5
                        if (*(nds_func3_cb_t **)(lVar9 + 0x20) != (nds_func3_cb_t *)0x0) {
                            //(**(code **)(lVar9 + 0x20))(&local_578,lVar9,1);
                            (**(nds_func3_cb_t **)(lVar9 + 0x20))(&local_578,lVar9,1);
                        }
                        if ((nds_func3_cb_t *)puVar12[1] != (nds_func3_cb_t *)0x0) {
                            //(*(code *)puVar12[1])(&local_578,puVar12,1);
                            (*(nds_func3_cb_t *)puVar12[1])(&local_578,puVar12,1);
                        }
                        puVar12 = (undefined8 *)puVar12[5];
                        if (puVar12 == (undefined8 *)0x0) {
                            puVar12 = local_568;
                            if (*(char *)(local_578 + 0x8ab38) != '\0') {
                                iStack_530 = 1;
                            }
                        }
                        else if ((nds_func3_cb_t *)puVar12[1] != (nds_func3_cb_t *)0x0) {
                            //(*(code *)puVar12[1])(&local_578,puVar12,0); // callback 함수 정의 필요함
                            (*(nds_func3_cb_t *)puVar12[1])(&local_578,puVar12,0);
                        }
                    }
                    goto LAB_000935e0;
                }
                iVar7 = -1;
            }
            iVar3 = *(int *)(puVar4 + 3); 
            printf("[trngaje] iVar3=0x%x\n", iVar3);
            if (*(nds_func3_cb_t **)(lVar9 + 0x20) != (nds_func3_cb_t *)0x0) {
                 printf("[trngaje] offset of lVar9 + 0x20=%p\n", lVar9 + 0x20 - base_addr_rx);
                //(**(code **)(lVar9 + 0x20))(&local_578,lVar9,1);
                (**(nds_func3_cb_t **)(lVar9 + 0x20))(&local_578,lVar9,1);
            }
            uVar5 = *(uint *)((long)puVar4 + 0x14) - 1;
            if (-1 < iVar7 + iVar3) {
                uVar5 = iVar7 + iVar3;
            }
            if (uVar5 < *(uint *)((long)puVar4 + 0x14)) {
                uVar11 = -(ulong)(uVar5 >> 0x1f) & 0xfffffff800000000 | (ulong)uVar5 << 3;
            }
            else {
                uVar5 = 0;
                uVar11 = 0;
            }
            lVar9 = *(long *)(puVar4[4] + uVar11);
            //pcVar10 = *(nds_func3_cb_t **)(lVar9 + 0x20);
            pcVar10 = *(nds_func3_cb_t **)(lVar9 + 0x20);

            printf("[trngaje] uVar5=0x%x, uVar11=0x%x\n", uVar5, uVar11);
            printf("[trngaje] offset of pcVar10=%p\n", pcVar10 - base_addr_rx);
            *(uint *)(puVar4 + 3) = uVar5; //  여기서 커서 위치 갱신 하는 부분 일  듯
            puVar12 = local_568;
            if (pcVar10 != (nds_func3_cb_t *)0x0) {
                (*pcVar10)(&local_578,lVar9,0); // callback 함수 정의 필요함
                puVar12 = local_568;
            }
        }
        goto LAB_000935e0; // loop 시작으로 
    }
    printf("[trngaje] sdl_menu:step19\n");
    audio_revert_pause_state(param_1 + 0x1586000,uVar6);
    do {
        get_gui_input(local_578 + 0x5528,local_580);
    } while (local_580[0] != 0xb);
    printf("[trngaje] sdl_menu:step20\n");
    clear_gui_actions();
    uVar11 = 0;
    if (*(int *)((long)__ptr + 0x14) != 0) {
        printf("[trngaje] sdl_menu:step21\n");
        do {
            while( true ) {
                pvVar13 = *(void **)(__ptr[4] + uVar11 * 8);
                if (*(nds_func2_cb_t **)((long)pvVar13 + 0x28) == (nds_func2_cb_t *)0x0) break;
                //(**(code **)((long)pvVar13 + 0x28))(&local_578,pvVar13); // callback 함수 정의 필요함
                (**(nds_func2_cb_t **)((long)pvVar13 + 0x28))(&local_578,pvVar13);

                uVar5 = (int)uVar11 + 1;
                uVar11 = (ulong)uVar5;
                free(pvVar13);
                if (*(uint *)((long)__ptr + 0x14) <= uVar5) goto LAB_00093834;
            }
            uVar5 = (int)uVar11 + 1;
            uVar11 = (ulong)uVar5;
            free(pvVar13);
        } while (uVar5 < *(uint *)((long)__ptr + 0x14));
    }
    printf("[trngaje] sdl_menu:step22\n");
LAB_00093834:
    printf("[trngaje] sdl_menu:step23\n");
    free((void *)__ptr[4]);
    free(__ptr);
    uVar5 = local_528;
    if (1 < local_528) {
        uVar5 = *(uint *)(param_1 + 0x859b4) | 2;
    }
    *(uint *)(param_1 + 0x859b4) = uVar5;
    uVar5 = local_524;
    if (local_524 != 0) {
        uVar5 = 0;
        // base_addr_rx + 0x00135150
        //if (*(uint *)(speed_override_values.11641 + (ulong)local_524 * 4) != 0) {
        if (*(uint *)((base_addr_rx + 0x00135150) + (ulong)local_524 * 4) != 0) {
            //uVar5 = 5000000 / *(uint *)(speed_override_values.11641 + (ulong)local_524 * 4);
            uVar5 = 5000000 / *(uint *)((base_addr_rx + 0x00135150) + (ulong)local_524 * 4);
        }
    }
    *(uint *)(param_1 + 0x859f0) = uVar5;
    *local_570 = (uint)(byte)local_520[0];
    local_570[1] = (uint)local_520[1];
    local_570[2] = (uint)local_520[2];
    local_570[3] = (uint)local_520[3];
    local_570[4] = (uint)local_520[4];
    local_570[5] = (uint)local_520[5];
    local_570[6] = (uint)local_520[6];
    local_570[7] = (uint)local_520[7];
    local_570[8] = (uint)local_518;
    local_570[9] = (uint)local_517;
    local_570[10] = (uint)local_516;
    printf("[trngaje] sdl_menu:step24\n");
    config_update_settings(puVar1);
    printf("[trngaje] sdl_menu:step25\n");
    set_screen_menu_off();
    if (local_540 != (void *)0x0) {
        free(local_540);
    }
    if (local_534 != 0) {
        printf("[trngaje] sdl_menu:step26\n");
        reset_system(param_1);
    }
    printf("[trngaje] sdl_menu:step27\n");
    // base_addr_rx + 003f6000
    nds_system = (unsigned char *)(base_addr_rx + 0x003f6000);
    nds_system[param_1 + 0x37339a3] = 1;
    printf("[trngaje] sdl_menu:step28\n");
    if (local_538 != 0) {
        printf("[trngaje] sdl_menu:step29\n");
        free(local_550);
        free(local_548);
        if (local_534 != 0) {
            printf("[trngaje] sdl_menu:step30\n");
            puts("Performing long jmp to reset.");
                    /* WARNING: Subroutine does not return */
            __longjmp_chk(param_1 + 0x3b29840,0);
        }
    }
  //if (local_8 != ___stack_chk_guard) {
                    /* WARNING: Subroutine does not return */
   // __stack_chk_fail();
  //}
    printf("[trngaje] sdl_menu--\n");
    return;	
}

//FUN_DRAW_NUMERIC_LABELED
void sdl_draw_numeric_labeled(long param_1,undefined8 *param_2,int param_3)
{
    undefined auStack_108 [256];

    nds_print_string                       print_string;
    
    print_string                    = (nds_print_string)FUN_PRINT_STRING;

    printf("[trngaje]sdl_draw_numeric_labeled: 0x%x, %s\n", *(uint *)param_2[6] , *(undefined8 *)(param_2[8] + (ulong)*(uint *)param_2[6] * 8));

    __sprintf_chk(auStack_108,1,0x100,"%s%s",*param_2,
                *(undefined8 *)(param_2[8] + (ulong)*(uint *)param_2[6] * 8),
                (ulong)*(uint *)param_2[6],0);
    if (param_3 != 0) {
        param_3 = 0x17;
    }
    print_string(auStack_108,0xffff,param_3,*(undefined4 *)(*(long *)(param_1 + 0x10) + 0x10),
               *(int *)(param_2 + 1) << 3);
}

/*
[trngaje] sdl_platform_get_config_input++
[trngaje] sdl_platform_get_config_input++1
[trngaje] sdl_platform_get_config_input:init
[trngaje] sdl_platform_get_config_input:DEVICE_TRIMUI
[trngaje] sdl_platform_get_config_input:windowID=0, event=2 is ignored
[trngaje] sdl_platform_get_config_input:init
[trngaje] sdl_platform_get_config_input:plus
[trngaje] sdl_platform_get_config_input: uVar2=32767
[trngaje] sdl_platform_get_config_input:DEVICE_TRIMUI
[trngaje] sdl_platform_get_config_input:windowID=0, event=2 is ignored
[trngaje] sdl_platform_get_config_input:uVar3=0x482(1154)
[trngaje] sdl_platform_get_config_input--
[trngaje] 0
[trngaje] get_current_menu_layer:0,352,Version r2.5.2.0
[trngaje] get_current_menu_layer:1,57,Enter Menu
[trngaje] 0
[trngaje] get_current_menu_layer:0,352,Version r2.5.2.0
[trngaje] get_current_menu_layer:1,57,Enter Menu
[trngaje] here: 9th parameter not 0
[trngaje] sdl_platform_get_config_input++
[trngaje] sdl_platform_get_config_input++1
[trngaje] sdl_platform_get_config_input:init
[trngaje] sdl_platform_get_config_input:DEVICE_TRIMUI
[trngaje] sdl_platform_get_config_input:windowID=0, event=5 is ignored
[trngaje] sdl_platform_get_config_input:init
[trngaje] sdl_platform_get_config_input:plus
[trngaje] sdl_platform_get_config_input: uVar2=32767
[trngaje] sdl_platform_get_config_input:DEVICE_TRIMUI
[trngaje] sdl_platform_get_config_input:windowID=0, event=5 is ignored

*/

ulong sdl_platform_get_config_input(void)
{
    int iVar1;
    uint uVar2;
    uint uVar3;
    uint uVar4;
    uint uVar5;
    int iVar6;
    long lVar7;
    long local_50;
    long local_48;
    SDL_Event event;
    //uint local_40 [2];
    //uint local_38;
    //byte local_34;
    //byte local_33;
    //short local_30;
    //uint local_2c;
    //long local_8; // not used
  
    nds_delay_us            delay_us;
    nds_get_ticks_us    get_ticks_us;

    delay_us                        = (nds_delay_us)FUN_DELAY_US;
    get_ticks_us                = (nds_get_ticks_us)FUN_GET_TICKS_US;
    printf("[trngaje] sdl_platform_get_config_input++\n");
    uVar3 = 0xffffffff;
    uVar4 = 0xffffffff;
    uVar5 = 0;
    //local_8 = ___stack_chk_guard;
    lVar7 = 0;
    iVar6 = 2;
    local_50 = 0;

    get_ticks_us(&local_48);
     printf("[trngaje] sdl_platform_get_config_input++1\n");
    while( true ) {
        get_ticks_us(&local_50);
         //printf("[trngaje] sdl_platform_get_config_input++2\n");
        //iVar1 = SDL_PollEvent(local_40);
         //iVar1 = SDL_PollEvent(&event);
         //printf("[trngaje] sdl_platform_get_config_input++3=iVar1=%d\n", iVar1);
        //while (iVar1 != 0) { // event is exist
        while (iVar1 = SDL_PollEvent(&event), iVar1 != 0 ) {
            //if (local_40[0] == 0x600) {
            if (event.type == SDL_JOYAXISMOTION) {
                //printf("[trngaje] sdl_platform_get_config_input++:step1\n");
                //if (-1 < (char)local_34) {
                if (-1 < (char)event.window.event) {
                    //printf("[trngaje] sdl_platform_get_config_input++:step1-1:event.window.event=%d\n", event.window.event);

                    if (event.window.windowID != uVar4) {
                        printf("[trngaje] sdl_platform_get_config_input:init\n");
                        uVar3 = 0xffffffff;
                        uVar5 = 0;
                    }

                    if (event.jaxis.value < 0) {
#ifdef DEVICE_TRIMUI
                        printf("[trngaje] sdl_platform_get_config_input:DEVICE_TRIMUI\n");
                         //  ignoreAxis- 02 / Axis- 05
                         if ((event.window.windowID == 0) &&(event.window.event == 2 || event.window.event == 5)) {
                            printf("[trngaje] sdl_platform_get_config_input:windowID=%d, event=%d is ignored\n", event.window.windowID, event.window.event );
                            // trimui 기기의 L2/R2 값이 plus -> minus 값이 연달아 들어 오는 경우 무시하도록 수정
                            // 첫 실행시에는 minus -> plus -> minus 로 들어오는 경우 존재함
                            continue;
                        }  
#endif
                        uVar2 = -(int)event.jaxis.value;
                        iVar1 = 3;  // minus
                    }
                    else if (event.jaxis.value > 0) {
                        printf("[trngaje] sdl_platform_get_config_input:plus\n");
                        iVar1 = 2; // plus
                        //uVar2 = (uint)local_30;
                        uVar2 = (uint)event.jaxis.value;
                    }
                    else {
                        // ignore
                         printf("[trngaje] sdl_platform_get_config_input:ignore center\n");
                        continue;
                    }

                    uVar4 = event.window.windowID;
                    if (((10000 < (int)uVar2 && uVar5 <= uVar2) && ((int)uVar2 < 0x2711 || uVar2 != uVar5)) &&
                        (uVar5 = uVar2, uVar3 != event.window.event || iVar6 != iVar1)) {
                        printf("[trngaje] sdl_platform_get_config_input: uVar2=%d\n", uVar2);
                        uVar3 = (uint)event.window.event;
                        lVar7 = local_50;
                        iVar6 = iVar1;
                    }
                }
            }
            //else if (local_40[0] < 0x601) {
            else if (event.type < 0x601) {
                //if (local_40[0] == 0x300) {
                if (event.type == SDL_KEYDOWN) {
                    //printf("[trngaje] sdl_platform_get_config_input++step2\n");
                    //uVar3 = ((int)local_2c >> 0x1e & 3U) << 8 | local_2c & 0xff;
                    uVar3 = ((int)event.window.data2 >> 0x1e & 3U) << 8 | event.window.data2 & 0xff; 
                    goto LAB_0009b378;
                }
            }
            else {
                //if (local_40[0] == 0x602) {
                if (event.type  == SDL_JOYHATMOTION) {   
                    //printf("[trngaje] sdl_platform_get_config_input++step3\n");
                    uVar3 = (event.window.windowID/*local_38*/ & 3) << 8 | event.jhat.value/*local_33*/ | 0x440;
                    goto LAB_0009b378;
                }
                //if (local_40[0] == 0x603) {
                if (event.type == SDL_JOYBUTTONDOWN) {
                     //printf("[trngaje] sdl_platform_get_config_input++step4\n");
                    uVar3 = (event.window.windowID/*local_38*/ & 3) << 8 | event.window.event/*local_34*/ | 0x400;
                    goto LAB_0009b378;
                }
            }
            //iVar1 = SDL_PollEvent(local_40);
            //iVar1 = SDL_PollEvent(&event);
        } // end of loop
        if ((uVar3 != 0xffffffff) && (500000 < (ulong)(local_50 - lVar7))) break;

        if (10000000 < (ulong)(local_50 - local_48)) {  // 10sec 이상 아무 입력이 없을 때
             //printf("[trngaje] sdl_platform_get_config_input++step6\n");
            uVar3 = 0xffff;
            LAB_0009b378:
                printf("[trngaje] sdl_platform_get_config_input--\n");
            return uVar3;
        }
        //delay_us("k");
        delay_us(100000);
    }

    uVar3 = (uVar4 & 3) << 8 | 0x400 | uVar3 | iVar6 << 6;

    printf("[trngaje] sdl_platform_get_config_input:uVar3=0x%x(%d)\n", uVar3, uVar3);
    goto LAB_0009b378;
}


void sdl_draw_menu_main(long *param1, long *param2) {
    printf("[trngaje] sdl_draw_menu_main++");
    
    printf("[trngaje] sdl_draw_menu_main--");
}

void sdl_draw_menu_option(long param_1,undefined8 *param_2,int param_3)
{
    nds_print_string                       print_string;
    
    print_string                    = (nds_print_string)FUN_PRINT_STRING;

    if (param_3 != 0) {
        param_3 = 0x17;
    }

    printf("[trngaje] sdl_draw_menu_option:x=%d\n", *(undefined4 *)(*(long *)(param_1 + 0x10) + 0x10)); // 이 변수 값 쓰레기 값이 채워져 있음
    print_string(*param_2,0xffff,param_3, 180/* *(undefined4 *)(*(long *)(param_1 + 0x10) + 0x10) */,
        *(int *)(param_2 + 1) << 3);
    return;
}

void process_displaylayout(int x, int y, int w, int h)
{
    SDL_Rect rtBG, rtS0, rtS1, rt;
    unsigned char  ucLayoutType;
    unsigned short  usLayoutRotate;
    char buf[256];
    char *bg;
    char *name;
    unsigned char ucLayoutIndex;

    usLayoutRotate = getlayout_rotate(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);
    ucLayoutType = getlayout_type(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);

    getlayout_size(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode], 0, &rtS0);
    getlayout_size(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode], 1, &rtS1);

    // draw bg
    rtBG.x = x;
    rtBG.y = y;
    rtBG.w = w;
    rtBG.h = h;
    SDL_FillRect(cvt, &rtBG, SDL_MapRGB(cvt->format, 0x00, 0x80, 0x00));

    if  (ucLayoutType == LAYOUT_TYPE_TRANSPARENT) {
        // draw screen1
        rt.x = x + w * rtS1.x / g_advdrastic.iDisplay_width;
        rt.y = y + h * rtS1.y / g_advdrastic.iDisplay_height;
        rt.w = w * rtS1.w / g_advdrastic.iDisplay_width;
        rt.h = h * rtS1.h / g_advdrastic.iDisplay_height;
        SDL_FillRect(cvt, &rt, SDL_MapRGB(cvt->format, 0x00, 0x00, 0x80));

        switch (nds.alpha.pos) {
        case 0:
            rtS0.x = g_advdrastic.iDisplay_width - rtS0.w;
            rtS0.y = 0;
            break;
        case 1:
            rtS0.x = 0;
            rtS0.y = 0;
            break;
        case 2:
            rtS0.x = 0;
            rtS0.y = g_advdrastic.iDisplay_height - rtS0.h;
            break;
        case 3:
            rtS0.x = g_advdrastic.iDisplay_width - rtS0.w;
            rtS0.y = g_advdrastic.iDisplay_height - rtS0.h;
            break;
        }

        // draw screen0
        rt.x = x + w * rtS0.x / g_advdrastic.iDisplay_width;
        rt.y = y + h * rtS0.y / g_advdrastic.iDisplay_height;
        rt.w = w * rtS0.w / g_advdrastic.iDisplay_width;
        rt.h = h * rtS0.h / g_advdrastic.iDisplay_height;
        SDL_FillRect(cvt, &rt, SDL_MapRGB(cvt->format, 0x80, 0x00, 0x00));
    }
    else if  (ucLayoutType == LAYOUT_TYPE_VERTICAL) {
        // draw screen0
        rt.x = x + w * rtS0.x / g_advdrastic.iDisplay_width;
        rt.y = y + h * rtS0.y / g_advdrastic.iDisplay_height;
        rt.w = w * rtS0.h / g_advdrastic.iDisplay_width;
        rt.h = h * rtS0.w / g_advdrastic.iDisplay_height;
        SDL_FillRect(cvt, &rt, SDL_MapRGB(cvt->format, 0x80, 0x00, 0x00));

        // draw screen1
        rt.x = x + w * rtS1.x / g_advdrastic.iDisplay_width;
        rt.y = y + h * rtS1.y / g_advdrastic.iDisplay_height;
        rt.w = w * rtS1.h / g_advdrastic.iDisplay_width;
        rt.h = h * rtS1.w / g_advdrastic.iDisplay_height;
        SDL_FillRect(cvt, &rt, SDL_MapRGB(cvt->format, 0x00, 0x00, 0x80));

    }
    else {
        // draw screen0
        rt.x = x + w * rtS0.x / g_advdrastic.iDisplay_width;
        rt.y = y + h * rtS0.y / g_advdrastic.iDisplay_height;
        rt.w = w * rtS0.w / g_advdrastic.iDisplay_width;
        rt.h = h * rtS0.h / g_advdrastic.iDisplay_height;
        SDL_FillRect(cvt, &rt, SDL_MapRGB(cvt->format, 0x80, 0x00, 0x00));

        // draw screen1
        rt.x = x + w * rtS1.x / g_advdrastic.iDisplay_width;
        rt.y = y + h * rtS1.y / g_advdrastic.iDisplay_height;
        rt.w = w * rtS1.w / g_advdrastic.iDisplay_width;
        rt.h = h * rtS1.h / g_advdrastic.iDisplay_height;
        SDL_FillRect(cvt, &rt, SDL_MapRGB(cvt->format, 0x00, 0x00, 0x80));
    }   

    // draw infos of layout
    bg = getlayout_bg(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);
    name = getlayout_name(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);
    ucLayoutIndex=getlayout_index(nds.hres_mode, g_advdrastic.ucLayoutIndex[nds.hres_mode]);

    sprintf(buf, "index:%d, name=%s", ucLayoutIndex,
        name);
    draw_info(cvt, buf, g_advdrastic.iDisplay_width * 0.5,  g_advdrastic.iDisplay_height * 0.5, nds.menu.c1, 0);

    sprintf(buf, "bg=%s", bg);
    draw_info(cvt, buf, g_advdrastic.iDisplay_width * 0.5,  g_advdrastic.iDisplay_height * 0.6, nds.menu.c1, 0);

    sprintf(buf, "s0:%dx%d, s1:%dx%d", rtS0.w, rtS0.h, rtS1.w, rtS1.h);
    draw_info(cvt, buf, g_advdrastic.iDisplay_width * 0.5,  g_advdrastic.iDisplay_height * 0.7, nds.menu.c1, 0);
}

void sdl_set_screen_menu_on(void)
{
    __printf_chk(1,"[trngaje] sdl_set_screen_menu_on: %d %d\n",800,0x1e0);
    //SDL_SetWindowFullscreen(DAT_03f2a578,1);
    //SDL_SetWindowFullscreen(*((uint64_t *)(base_addr_rx + 0x03f2a578)), 1); 

    //SDL_RenderSetLogicalSize(DAT_03f2a580,800,0x1e0);
    SDL_RenderSetLogicalSize(*((uint64_t *)(base_addr_rx + 0x03f2a580)) , 800, 0x1e0);

    //if (DAT_03f2a588 == 0) { 
    if (*((uint64_t *)(base_addr_rx + 0x03f2a588)) == 0) { 
        *((uint64_t *)(base_addr_rx + 0x03f2a588))  /*DAT_03f2a588*/ = SDL_CreateTexture(*((uint64_t *)(base_addr_rx + 0x03f2a588)) /*DAT_03f2a580*/, 0x15151002, 1, 800, 0x1e0);
    }
    //clear_screen();
    if (*((uint64_t *)(base_addr_rx + 0x03f2a5a0)) /*DAT_03f2a5a0*/ != (void *)0x0) { 
        *((uint64_t *)(base_addr_rx + 0x03f2a5e0)) /*DAT_03f2a5e0*/ = 1;
    return;
    }
    *((uint64_t *)(base_addr_rx + 0x03f2a5a0)) /*DAT_03f2a5a0*/ = malloc(0xbb800);
    *((uint64_t *)(base_addr_rx + 0x03f2a5e0)) /*DAT_03f2a5e0*/ = 1;
    return;
}