#ifndef _JSONLAYOUT_H
#define _JSONLAYOUT_H

#include "SDL_rect.h"

#define MAX_LAYOUTS 32

typedef struct {
    int index;
    int type;           /* 0:normal, 1:transparent, 2:vertical, 3:high resolution, 4:single */
    char *name;
    char *bg;           /* background image filename */
    int rotate;         /* rotation angle (0, 90, 180, 270) */
    SDL_Rect screen[2]; /* screen0 and screen1 rects */
} nds_layout_t;

typedef struct {
    char *name;
    int num_layouts;
    nds_layout_t layouts[MAX_LAYOUTS];
} nds_theme_t;

int nds_layout_load(const char *json_path);
void nds_layout_free(void);
int nds_layout_get_count(void);
nds_layout_t *nds_layout_get(int index);
const char *nds_layout_get_bg_path(int index);

/* Settings functions */
void nds_settings_set_path(const char *path);
int nds_settings_load_position(void);
void nds_settings_save_position(int position);

#endif /* _JSONLAYOUT_H */
