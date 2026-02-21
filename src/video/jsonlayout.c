// 이 프로그램은 drastic layout 출력을 위해서 설정 값 관리를 위한 루틴입니다.

#include "jsonlayout.h"

#include <stdio.h>
#include <string.h>
#include <json-c/json.h>

theme_t g_theme;

int getjsonlayout_initialize(const char *strjsonlayoutfile)
{
    json_object * pJsonObject = NULL;
    json_object * pJsonObject2 = NULL;

    json_object * pJsonObject3 = NULL;
    json_object * pJsonObject4 = NULL;

    int type = 0;
    int i, ii;
    int ihighres;
    int iLayout_type;

    pJsonObject = json_object_from_file(strjsonlayoutfile);
    printf("[trngaje] strjsonlayoutfile=%s\n", strjsonlayoutfile);

	if ( pJsonObject == NULL ) {
		printf("Reading file object is failed.");
		return -1;
	}

    // initialize internal variables
    memset(&g_theme, 0, sizeof(g_theme));

	pJsonObject2 = json_object_object_get(pJsonObject,"name");

	//type = json_object_get_type(pJsonObject2);
    if  (pJsonObject2 != NULL) {
        g_theme.name = strdup( json_object_get_string(pJsonObject2));
    }

	pJsonObject2 = json_object_object_get(pJsonObject,"number");
    if  (pJsonObject2 != NULL) {
        type = json_object_get_type(pJsonObject2);
        g_theme.number = json_object_get_int(pJsonObject2);
    }

	pJsonObject2 = json_object_object_get(pJsonObject,"layout");
	type = json_object_get_type(pJsonObject2);
    if( type != json_type_array) return -1;

    g_theme.num_of_layout = json_object_array_length(pJsonObject2) ;
    if ( g_theme.num_of_layout > 20)
        g_theme.num_of_layout  = 20;

	for( ii = 0 ; ii < g_theme.num_of_layout ; ii++)
	{
		pJsonObject3 = json_object_array_get_idx(pJsonObject2,ii);
        if  (pJsonObject3 == NULL)
            return -1;
        

        iLayout_type = 0;
        pJsonObject4 = json_object_object_get(pJsonObject3,"type");
         if  (pJsonObject4 != NULL) {
            iLayout_type = json_object_get_int(pJsonObject4);
            //g_theme.layout[i].type = json_object_get_int(pJsonObject4);
        }
    
        if (iLayout_type == 3)
            ihighres = 1;
        else
            ihighres = 0;

        i = g_theme.max_layout[ihighres];

        g_theme.layout[ihighres][i].type = iLayout_type;

        pJsonObject4 = json_object_object_get(pJsonObject3,"index");
         if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].index = json_object_get_int(pJsonObject4);
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"name");
        if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].name = strdup(json_object_get_string(pJsonObject4));
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"bg");
         if  (pJsonObject4 != NULL) {
            if (strlen(json_object_get_string(pJsonObject4)) > 0) {
                g_theme.layout[ihighres][i].bg = strdup(json_object_get_string(pJsonObject4));
                printf("[trngaje] %d, len=%d\n", i, strlen(g_theme.layout[ihighres][i].bg));
            }
            else
                g_theme.layout[ihighres][i].bg =  NULL;
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"rotate");
         if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].rotate = json_object_get_int(pJsonObject4);
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"screen0_x");
         if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].screen[0].x = json_object_get_int(pJsonObject4);
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"screen0_y");
         if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].screen[0].y = json_object_get_int(pJsonObject4);
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"screen0_w");
        if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].screen[0].w = json_object_get_int(pJsonObject4);
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"screen0_h");
         if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].screen[0].h= json_object_get_int(pJsonObject4);
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"screen1_x");
        if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].screen[1].x = json_object_get_int(pJsonObject4);
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"screen1_y");
        if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].screen[1].y = json_object_get_int(pJsonObject4);
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"screen1_w");
        if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].screen[1].w = json_object_get_int(pJsonObject4);
        }

        pJsonObject4 = json_object_object_get(pJsonObject3,"screen1_h");
        if  (pJsonObject4 != NULL) {
            g_theme.layout[ihighres][i].screen[1].h= json_object_get_int(pJsonObject4);
        }
        
        // increment 
        g_theme.max_layout[ihighres] += 1;
	}

    json_object_put(pJsonObject);
}

void printjsonlayout(void)
{
    int i, ihighres;
    printf("name=%s\n", g_theme.name);
    printf("number=%d\n", g_theme.number);
    printf("num_of_layout=%d(normal:%d, high res:%d\n", g_theme.num_of_layout, g_theme.max_layout[0], g_theme.max_layout[1]);

    for(ihighres=0; ihighres<2; ihighres++) {
        for(i=0; i<g_theme.max_layout[ihighres]/*g_theme.num_of_layout*/; i++) {
            printf("[%d][%d] index=%d\n", ihighres, i, g_theme.layout[ihighres][i].index);
            printf("[%d][%d] name=%s\n", ihighres, i, g_theme.layout[ihighres][i].name);
            printf("[%d][%d] bg=%s\n", ihighres, i, g_theme.layout[ihighres][i].bg);
            printf("[%d][%d] rotate=%d\n",ihighres, i, g_theme.layout[ihighres][i].rotate);
            printf("[%d][%d] screen[0].x=%d\n", ihighres, i, g_theme.layout[ihighres][i].screen[0].x);
            printf("[%d][%d] screen[0].y=%d\n", ihighres, i, g_theme.layout[ihighres][i].screen[0].y);
            printf("[%d][%d] screen[0].w=%d\n", ihighres, i, g_theme.layout[ihighres][i].screen[0].w);
            printf("[%d][%d] screen[0].h=%d\n", ihighres, i, g_theme.layout[ihighres][i].screen[0].h);

            printf("[%d][%d] screen[1].x=%d\n", ihighres, i, g_theme.layout[ihighres][i].screen[1].x);
            printf("[%d][%d] screen[1].y=%d\n", ihighres, i, g_theme.layout[ihighres][i].screen[1].y);
            printf("[%d][%d] screen[1].w=%d\n", ihighres, i, g_theme.layout[ihighres][i].screen[1].w);
            printf("[%d][%d] screen[1].h=%d\n", ihighres, i, g_theme.layout[ihighres][i].screen[1].h);
        }
    }
}

char *getlayout_name(int ihighres, int i)
{
    return (g_theme.layout[ihighres][i].name);
}

char *getlayout_bg(int ihighres, int i)
{
    return (g_theme.layout[ihighres][i].bg);
}

int getmax_layout(int ihighres)
{
    return (g_theme.max_layout[ihighres]);
}

void getlayout_size(int ihighres, int i, int screenid, SDL_Rect * rt)
{
    if (rt == NULL) 
        return;

    if (i >= g_theme.num_of_layout)
        return;

    if (screenid >= 2 || screenid < 0)
        return;

    if (ihighres >= 2 || ihighres < 0)
        return;

    rt->x =  g_theme.layout[ihighres][i].screen[screenid].x;
    rt->y =  g_theme.layout[ihighres][i].screen[screenid].y;
    rt->w =  g_theme.layout[ihighres][i].screen[screenid].w;
    rt->h =  g_theme.layout[ihighres][i].screen[screenid].h;
    
}

unsigned char getlayout_type(int ihighres, int i)
{
    return ((unsigned char)(g_theme.layout[ihighres][i].type));
}

unsigned short getlayout_rotate(int ihighres, int i)
{
    return ((unsigned short)(g_theme.layout[ihighres][i].rotate));
}

unsigned char getlayout_index(int ihighres, int i)
{
    return ((unsigned char)(g_theme.layout[ihighres][i].index));
}

#if 0
void main(void)
{
    getjsonlayout_initialize("./layout.json");
    printjsonlayout();
}
#endif