#include "jsonlayout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

static nds_theme_t g_theme = {0};
static char g_layout_dir[256] = {0};
static char g_settings_path[256] = {0};

int nds_layout_load(const char *json_path)
{
    json_object *jfile = NULL;
    json_object *jval = NULL;
    json_object *jlayout = NULL;
    json_object *jitem = NULL;
    int i, count;
    char *dir_end;

    nds_layout_free();

    strncpy(g_layout_dir, json_path, sizeof(g_layout_dir) - 1);
    dir_end = strrchr(g_layout_dir, '/');
    if (dir_end) {
        *(dir_end + 1) = '\0';
    } else {
        g_layout_dir[0] = '\0';
    }

    jfile = json_object_from_file(json_path);
    if (!jfile) {
        printf("jsonlayout: failed to load %s\n", json_path);
        return -1;
    }

    if (json_object_object_get_ex(jfile, "name", &jval)) {
        g_theme.name = strdup(json_object_get_string(jval));
    }

    if (!json_object_object_get_ex(jfile, "layout", &jlayout)) {
        printf("jsonlayout: no layout array found\n");
        json_object_put(jfile);
        return -1;
    }

    if (json_object_get_type(jlayout) != json_type_array) {
        printf("jsonlayout: layout is not an array\n");
        json_object_put(jfile);
        return -1;
    }

    count = json_object_array_length(jlayout);
    if (count > MAX_LAYOUTS)
        count = MAX_LAYOUTS;

    for (i = 0; i < count; i++) {
        nds_layout_t *layout = &g_theme.layouts[g_theme.num_layouts];

        jitem = json_object_array_get_idx(jlayout, i);
        if (!jitem)
            continue;

        if (json_object_object_get_ex(jitem, "index", &jval))
            layout->index = json_object_get_int(jval);

        if (json_object_object_get_ex(jitem, "type", &jval))
            layout->type = json_object_get_int(jval);

        if (json_object_object_get_ex(jitem, "name", &jval))
            layout->name = strdup(json_object_get_string(jval));

        if (json_object_object_get_ex(jitem, "bg", &jval)) {
            const char *bg = json_object_get_string(jval);
            if (bg && strlen(bg) > 0)
                layout->bg = strdup(bg);
        }

        if (json_object_object_get_ex(jitem, "rotate", &jval))
            layout->rotate = json_object_get_int(jval);

        if (json_object_object_get_ex(jitem, "screen0_x", &jval))
            layout->screen[0].x = json_object_get_int(jval);
        if (json_object_object_get_ex(jitem, "screen0_y", &jval))
            layout->screen[0].y = json_object_get_int(jval);
        if (json_object_object_get_ex(jitem, "screen0_w", &jval))
            layout->screen[0].w = json_object_get_int(jval);
        if (json_object_object_get_ex(jitem, "screen0_h", &jval))
            layout->screen[0].h = json_object_get_int(jval);

        if (json_object_object_get_ex(jitem, "screen1_x", &jval))
            layout->screen[1].x = json_object_get_int(jval);
        if (json_object_object_get_ex(jitem, "screen1_y", &jval))
            layout->screen[1].y = json_object_get_int(jval);
        if (json_object_object_get_ex(jitem, "screen1_w", &jval))
            layout->screen[1].w = json_object_get_int(jval);
        if (json_object_object_get_ex(jitem, "screen1_h", &jval))
            layout->screen[1].h = json_object_get_int(jval);

        g_theme.num_layouts++;
    }

    json_object_put(jfile);

    printf("jsonlayout: loaded %d layouts from %s\n", g_theme.num_layouts, json_path);
    return 0;
}

void nds_layout_free(void)
{
    int i;

    if (g_theme.name) {
        free(g_theme.name);
        g_theme.name = NULL;
    }

    for (i = 0; i < g_theme.num_layouts; i++) {
        if (g_theme.layouts[i].name) {
            free(g_theme.layouts[i].name);
            g_theme.layouts[i].name = NULL;
        }
        if (g_theme.layouts[i].bg) {
            free(g_theme.layouts[i].bg);
            g_theme.layouts[i].bg = NULL;
        }
    }

    g_theme.num_layouts = 0;
    g_layout_dir[0] = '\0';
}

int nds_layout_get_count(void)
{
    return g_theme.num_layouts;
}

nds_layout_t *nds_layout_get(int index)
{
    if (index < 0 || index >= g_theme.num_layouts)
        return NULL;
    return &g_theme.layouts[index];
}

const char *nds_layout_get_bg_path(int index)
{
    static char path_buf[512];
    nds_layout_t *layout;

    if (index < 0 || index >= g_theme.num_layouts)
        return NULL;

    layout = &g_theme.layouts[index];
    if (!layout->bg || strlen(layout->bg) == 0)
        return NULL;

    /* Background images are in subdirectory "1" (hardcoded theme index) */
    snprintf(path_buf, sizeof(path_buf), "%s1/%s", g_layout_dir, layout->bg);
    return path_buf;
}

void nds_settings_set_path(const char *path)
{
    if (path)
        strncpy(g_settings_path, path, sizeof(g_settings_path) - 1);
    else
        g_settings_path[0] = '\0';
}

int nds_settings_load_position(void)
{
    json_object *jfile, *jval;
    int position = 0;

    if (g_settings_path[0] == '\0')
        return 0;

    jfile = json_object_from_file(g_settings_path);
    if (!jfile)
        return 0;

    if (json_object_object_get_ex(jfile, "position", &jval))
        position = json_object_get_int(jval);

    json_object_put(jfile);
    printf("Loaded position=%d from %s\n", position, g_settings_path);
    return position;
}

void nds_settings_save_position(int position)
{
    json_object *jfile, *jval;

    if (g_settings_path[0] == '\0')
        return;

    jfile = json_object_from_file(g_settings_path);
    if (!jfile) {
        jfile = json_object_new_object();
        if (!jfile)
            return;
    }

    json_object_object_del(jfile, "position");
    jval = json_object_new_int(position);
    json_object_object_add(jfile, "position", jval);

    if (json_object_to_file(g_settings_path, jfile) < 0)
        printf("Failed to save settings to %s\n", g_settings_path);
    else
        printf("Saved position=%d to %s\n", position, g_settings_path);

    json_object_put(jfile);
}

int nds_settings_load_alpha(void)
{
    json_object *jfile, *jval;
    int alpha = -1;

    if (g_settings_path[0] == '\0')
        return -1;

    jfile = json_object_from_file(g_settings_path);
    if (!jfile)
        return -1;

    if (json_object_object_get_ex(jfile, "alpha", &jval))
        alpha = json_object_get_int(jval);

    json_object_put(jfile);
    if (alpha >= 0)
        printf("Loaded alpha=%d from %s\n", alpha, g_settings_path);
    return alpha;
}

void nds_settings_save_alpha(int alpha)
{
    json_object *jfile, *jval;

    if (g_settings_path[0] == '\0')
        return;

    jfile = json_object_from_file(g_settings_path);
    if (!jfile) {
        jfile = json_object_new_object();
        if (!jfile)
            return;
    }

    json_object_object_del(jfile, "alpha");
    jval = json_object_new_int(alpha);
    json_object_object_add(jfile, "alpha", jval);

    if (json_object_to_file(g_settings_path, jfile) < 0)
        printf("Failed to save settings to %s\n", g_settings_path);
    else
        printf("Saved alpha=%d to %s\n", alpha, g_settings_path);

    json_object_put(jfile);
}
