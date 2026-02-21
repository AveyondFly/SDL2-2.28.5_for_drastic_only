#ifndef _JSONLAYOUT_H
#define _JSONLAYOUT_H

#include "SDL_rect.h"

typedef struct {
    int x;
    int y;
    int w;
    int h;
}  screen_t;

/*
 enum LAYOUT_TYPE_ENUM {
    LAYOUT_TYPE_NORMAL=0, 
    LAYOUT_TYPE_TRANSPARENT,
    LAYOUT_TYPE_VERTICAL, 
    LAYOUT_TYPE_HIGHRESOLUTION,
    LAYOUT_TYPE_SINGLE,

    NUM_OF_LAYOUT_TYPE
};

*/

typedef struct {
    int index;         // for debug
    int type;           // 0:normal, 1:trans, 2:vertical (if 2 or rotate is not 0), 3:high resolution, 4:single screen 
    char *name;   // frendly name
    char *bg;         // name of file 
    int rotate;        // rotate information
    screen_t screen[2];
} layout_t;

typedef struct {
    char *name; // name of authon
    int number; // for loop, not used
    int num_of_layout;      // total number of layout that includes in json file 
    int max_layout[2];         // 0 : normal, 1: high resolution
    layout_t layout[2][20]; // 0 : normal, 1: high resolution
} theme_t;

int getjsonlayout_initialize(const char *strjsonlayoutfile);
void printjsonlayout(void);
char *getlayout_name(int ihighres, int i);
char *getlayout_bg(int ihighres, int i);
void getlayout_size(int ihighres, int i, int screenid, SDL_Rect * rt);
unsigned char getlayout_type(int ihighres, int i);
unsigned short getlayout_rotate(int ihighres, int i);
unsigned char getlayout_index(int ihighres, int i);
int getmax_layout(int ihighres);

#endif