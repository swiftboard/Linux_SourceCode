/*
 * \file        df_view.c
 * \brief       
 *
 * \version     1.0.0
 * \date        2012年05月31日
 * \author      James Deng <csjamesdeng@allwinnertech.com>
 *
 * Copyright (c) 2012 Allwinner Technology. All Rights Reserved.
 *
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <directfb.h>

#include "drv_display_sun4i.h"
#include "dragonboard.h"
#include "script.h"
#include "view.h"
#include "test_case.h"
#include "tp_track.h"
#include "df_view.h"

#define DATADIR                         "/dragonboard/fonts"

#define FONT16_HEIGHT                   16
#define FONT20_HEIGHT                   20
#define FONT24_HEIGHT                   24
#define DEFAULT_FONT_SIZE               FONT20_HEIGHT

#define MENU_HEIGHT                     32
#define ITEM_HEIGHT                     28

#define HEIGHT_ADJUST                   (2 * ITEM_HEIGHT)

#define AUTO_MENU_NAME                  "自动测试项"
#define MANUAL_MENU_NAME                "手动测试项"
#define COPYRIGHT                       "DragonBoard V1.0.0 Copyright (C) 2012 Allwinner"
#define CLEAR_BUTTON_NAME               "清屏"

IDirectFB              *dfb;
IDirectFBDisplayLayer  *layer;

IDirectFBWindow        *auto_tc_window;
IDirectFBWindow        *manual_tc_window;
IDirectFBWindow        *video_window;
IDirectFBWindow        *misc_window;
IDirectFBSurface       *auto_tc_surface;
IDirectFBSurface       *manual_tc_surface;
IDirectFBSurface       *video_surface;
IDirectFBSurface       *misc_surface;

IDirectFBFont          *font16;
IDirectFBFont          *font20;
IDirectFBFont          *font24;
IDirectFBFont          *font;

DFBDisplayLayerConfig           layer_config;
DFBGraphicsDeviceDescription    gdesc;

/* input device: tp */
static IDirectFBInputDevice *tp = NULL;

/* input interfaces: device and its buffer */
static IDirectFBEventBuffer *events;

static int auto_tc_window_width;
static int auto_tc_window_height;

static int manual_tc_window_width;
static int manual_tc_window_height;

static int df_view_id;
static int font_size;
static int height_adjust;

static pthread_t evt_tid;

static struct list_head auto_tc_list;
static struct list_head manual_tc_list;

struct color_rgb color_tables[8] = 
{
    {0xff, 0xff, 0xff},
    {0xff, 0xff, 0x00},
    {0x00, 0xff, 0x00},
    {0x00, 0xff, 0xff},
    {0xff, 0x00, 0xff},
    {0xff, 0x00, 0x00},
    {0x00, 0x00, 0xff},
    {0x00, 0x00, 0x00}
};

static int menu_bgcolor;
static int menu_fgcolor;
static int item_init_bgcolor;
static int item_init_fgcolor;
static int item_ok_bgcolor;
static int item_ok_fgcolor;
static int item_fail_bgcolor;
static int item_fail_fgcolor;

static char pass_str[12];
static char fail_str[12];

#define BUILDIN_TC_ID_TP                -1

#define TP_DISPLAY_NAME                 "触摸"

static struct item_data tp_data;

static __disp_rect_t btn_rect;

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...) \
    do {                                                             \
        DFBResult err = x;                                           \
        if (err != DFB_OK) {                                         \
             fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );  \
             DirectFBErrorFatal( #x, err );                          \
        }                                                            \
    } while (0)

#define CLAMP(x, low, high) \
    (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

static int auto_tc_window_init(void)
{
    DFBWindowDescription    desc;
    char                    menu_name[36];

    auto_tc_window_width  = layer_config.width >> 1;
    auto_tc_window_height = (layer_config.height >> 1) - height_adjust;

    desc.flags  = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT 
            | DWDESC_CAPS);

    desc.posx   = 0;
    desc.posy   = (layer_config.height >> 1) + height_adjust;
    desc.width  = auto_tc_window_width;
    desc.height = auto_tc_window_height;
    desc.caps   = DWCAPS_ALPHACHANNEL;

    DFBCHECK(layer->CreateWindow(layer, &desc, &auto_tc_window));
    auto_tc_window->GetSurface(auto_tc_window, &auto_tc_surface);
    auto_tc_window->SetOpacity(auto_tc_window, 0xff);
    auto_tc_window->SetOptions(auto_tc_window, DWOP_KEEP_POSITION);

    DFBCHECK(auto_tc_surface->SetColor(auto_tc_surface, 
                                       color_tables[item_init_bgcolor].r, 
                                       color_tables[item_init_bgcolor].g, 
                                       color_tables[item_init_bgcolor].b, 0xff));
    DFBCHECK(auto_tc_surface->FillRectangle(auto_tc_surface, 0, 0, desc.width, desc.height));

    /* draw menu */
    if (script_fetch("df_view", "auto_menu_name", (int *)menu_name, sizeof(menu_name) / 4 - 1)) {
        strcpy(menu_name, AUTO_MENU_NAME);
    }

    DFBCHECK(auto_tc_surface->SetColor(auto_tc_surface, 
                                       color_tables[menu_bgcolor].r, 
                                       color_tables[menu_bgcolor].g, 
                                       color_tables[menu_bgcolor].b, 0xff));
    DFBCHECK(auto_tc_surface->FillRectangle(auto_tc_surface, 0, 0, desc.width, MENU_HEIGHT));
    DFBCHECK(auto_tc_surface->SetFont(auto_tc_surface, font24));
    DFBCHECK(auto_tc_surface->SetColor(auto_tc_surface, 
                                       color_tables[menu_fgcolor].r, 
                                       color_tables[menu_fgcolor].g,
                                       color_tables[menu_fgcolor].b, 0xff));
    DFBCHECK(auto_tc_surface->DrawString(auto_tc_surface, menu_name, -1, 4, 
                MENU_HEIGHT / 2 + FONT24_HEIGHT / 2 - FONT24_HEIGHT / 8, DSTF_LEFT));

    return 0;
}

static int draw_clear_button(void)
{
    char btn_name[12];

    if (script_fetch("df_view", "clear_button_name", (int *)btn_name, sizeof(btn_name) / 4 - 1)) {
        strcpy(btn_name, CLEAR_BUTTON_NAME);
    }
    btn_rect.width  = MENU_HEIGHT * 3;
    btn_rect.height = MENU_HEIGHT;
    btn_rect.x      = manual_tc_window_width - btn_rect.width;
    btn_rect.y      = 0;
    DFBCHECK(manual_tc_surface->SetColor(manual_tc_surface, 0xff, 0, 0, 0xff));
    DFBCHECK(manual_tc_surface->DrawLine(manual_tc_surface, btn_rect.x, 
                                                            btn_rect.y,
                                                            btn_rect.x,
                                                            btn_rect.y + btn_rect.height - 1));
    DFBCHECK(manual_tc_surface->DrawLine(manual_tc_surface, btn_rect.x, 
                                                            btn_rect.y,
                                                            btn_rect.x + btn_rect.width - 1,
                                                            btn_rect.y));
    DFBCHECK(manual_tc_surface->DrawLine(manual_tc_surface, btn_rect.x + btn_rect.width - 1, 
                                                            btn_rect.y,
                                                            btn_rect.x + btn_rect.width - 1,
                                                            btn_rect.y + btn_rect.height - 1));
    DFBCHECK(manual_tc_surface->DrawLine(manual_tc_surface, btn_rect.x, 
                                                            btn_rect.y + btn_rect.height - 1,
                                                            btn_rect.x + btn_rect.width - 1,
                                                            btn_rect.y + btn_rect.height - 1));

    DFBCHECK(manual_tc_surface->SetColor(manual_tc_surface, 0, 0, 0xff, 0xff));
    DFBCHECK(manual_tc_surface->FillRectangle(manual_tc_surface, btn_rect.x + 1,
                                                                 btn_rect.y + 1,
                                                                 btn_rect.width - 2,
                                                                 btn_rect.height - 2));

    DFBCHECK(manual_tc_surface->SetColor(manual_tc_surface, 0xff, 0xff, 0xff, 0xff));
    DFBCHECK(manual_tc_surface->DrawString(manual_tc_surface, btn_name, -1, btn_rect.x + (btn_rect.width >> 1),
                MENU_HEIGHT / 2 + FONT24_HEIGHT / 2 - FONT24_HEIGHT / 8, DSTF_CENTER));

    return 0;
}

static int manual_tc_window_init(void)
{
    DFBWindowDescription    desc;
    char                    menu_name[36];

    manual_tc_window_width  = layer_config.width >> 1;
    manual_tc_window_height = (layer_config.height >> 1) + height_adjust;

    desc.flags  = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT 
            | DWDESC_CAPS);

    desc.posx   = 0;
    desc.posy   = 0;
    desc.width  = manual_tc_window_width;
    desc.height = manual_tc_window_height;
    desc.caps   = DWCAPS_ALPHACHANNEL;

    DFBCHECK(layer->CreateWindow(layer, &desc, &manual_tc_window));
    manual_tc_window->GetSurface(manual_tc_window, &manual_tc_surface);
    manual_tc_window->SetOpacity(manual_tc_window, 0xff);
    manual_tc_window->SetOptions(manual_tc_window, DWOP_KEEP_POSITION);

    DFBCHECK(manual_tc_surface->SetColor(manual_tc_surface, 
                                         color_tables[item_init_bgcolor].r, 
                                         color_tables[item_init_bgcolor].g, 
                                         color_tables[item_init_bgcolor].b, 0xff));
    DFBCHECK(manual_tc_surface->FillRectangle(manual_tc_surface, 0, 0, desc.width, desc.height));

    /* draw menu */
    if (script_fetch("df_view", "manual_menu_name", (int *)menu_name, sizeof(menu_name) / 4 - 1)) {
        strcpy(menu_name, MANUAL_MENU_NAME);
    }
    DFBCHECK(manual_tc_surface->SetColor(manual_tc_surface, 
                                         color_tables[menu_bgcolor].r, 
                                         color_tables[menu_bgcolor].g, 
                                         color_tables[menu_bgcolor].b, 0xff));
    DFBCHECK(manual_tc_surface->FillRectangle(manual_tc_surface, 0, 0, desc.width, MENU_HEIGHT));
    DFBCHECK(manual_tc_surface->SetFont(manual_tc_surface, font24));
    DFBCHECK(manual_tc_surface->SetColor(manual_tc_surface, 
                                         color_tables[menu_fgcolor].r,
                                         color_tables[menu_fgcolor].g, 
                                         color_tables[menu_fgcolor].b, 0xff));
    DFBCHECK(manual_tc_surface->DrawString(manual_tc_surface, menu_name, -1, 4, 
                MENU_HEIGHT / 2 + FONT24_HEIGHT / 2 - FONT24_HEIGHT / 8, DSTF_LEFT));

    /* draw clear button */
    draw_clear_button();

    auto_tc_surface->Flip(auto_tc_surface, NULL, 0);

    manual_tc_surface->Flip(manual_tc_surface, NULL, 0);

    return 0;
}

static int video_window_init(void)
{
    DFBWindowDescription    desc;

    desc.flags  = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT 
            | DWDESC_CAPS);

    desc.posx   = layer_config.width >> 1;
    desc.posy   = 0;
    desc.width  = layer_config.width >> 1;
    desc.height = layer_config.height >> 1;
    desc.caps   = DWCAPS_ALPHACHANNEL;

    DFBCHECK(layer->CreateWindow(layer, &desc, &video_window));
    video_window->GetSurface(video_window, &video_surface);
    video_window->SetOpacity(video_window, 0xff);
    video_window->SetOptions(video_window, DWOP_KEEP_POSITION);

    DFBCHECK(video_surface->SetColor(video_surface, 0, 0, 0, 0xff));
    DFBCHECK(video_surface->FillRectangle(video_surface, 0, 0, desc.width, desc.height));

    video_surface->Flip(video_surface, NULL, 0);

    return 0;
}

static int misc_window_init(void)
{
    DFBWindowDescription    desc;
    int rwidth, gwidth, bxpos, bwidth;

    desc.flags  = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT 
            | DWDESC_CAPS);

    desc.posx   = layer_config.width >> 1;
    desc.posy   = layer_config.height >> 1;
    desc.width  = layer_config.width >> 1;
    desc.height = layer_config.height >> 1;
    desc.caps   = DWCAPS_ALPHACHANNEL;

    DFBCHECK(layer->CreateWindow(layer, &desc, &misc_window));
    misc_window->GetSurface(misc_window, &misc_surface);
    misc_window->SetOpacity(misc_window, 0xff);
    misc_window->SetOptions(misc_window, DWOP_KEEP_POSITION);

    /* draw RGB */
    rwidth = gwidth = desc.width / 3;
    bxpos = rwidth + gwidth;
    bwidth = desc.width - bxpos;
    DFBCHECK(misc_surface->SetColor(misc_surface, 0xff, 0, 0, 0xff));
    DFBCHECK(misc_surface->FillRectangle(misc_surface, 0, 0, rwidth, desc.height));
    DFBCHECK(misc_surface->SetColor(misc_surface, 0, 0xff, 0, 0xff));
    DFBCHECK(misc_surface->FillRectangle(misc_surface, rwidth, 0, gwidth, desc.height));
    DFBCHECK(misc_surface->SetColor(misc_surface, 0, 0, 0xff, 0xff));
    DFBCHECK(misc_surface->FillRectangle(misc_surface, bxpos, 0, bwidth, desc.height));

    /* draw copryright */
    DFBCHECK(misc_surface->SetFont(misc_surface, font16));
    DFBCHECK(misc_surface->SetColor(misc_surface, 0, 0, 0, 0xff));
    DFBCHECK(misc_surface->DrawString(misc_surface, COPYRIGHT, -1, desc.width - 4, 
                desc.height - FONT16_HEIGHT / 4, DSTF_RIGHT));

    misc_surface->Flip(misc_surface, NULL, 0);

    return 0;
}

static int df_font_init(void)
{
    DFBFontDescription font_desc;

    if (script_fetch("df_view", "font_size", &font_size, 1) || 
            (font_size != FONT20_HEIGHT && font_size != FONT24_HEIGHT)) {
        font_size = DEFAULT_FONT_SIZE;
    }

    /* create font 16 pixel */
    font_desc.flags  = DFDESC_HEIGHT;
    font_desc.height = FONT16_HEIGHT;
    DFBCHECK(dfb->CreateFont(dfb, DATADIR"/wqy-zenhei.ttc", &font_desc, &font16));

    /* create font 20 pixel */
    font_desc.flags  = DFDESC_HEIGHT;
    font_desc.height = FONT20_HEIGHT;
    DFBCHECK(dfb->CreateFont(dfb, DATADIR"/wqy-zenhei.ttc", &font_desc, &font20));

    /* create font 24 pixel */
    font_desc.flags  = DFDESC_HEIGHT;
    font_desc.height = FONT24_HEIGHT;
    DFBCHECK(dfb->CreateFont(dfb, DATADIR"/wqy-zenhei.ttc", &font_desc, &font24));

    if (font_size == FONT24_HEIGHT) {
        font = font24;
    }
    else {
        font = font20;
    }

    return 0;
}

static int df_config_init(void)
{
    if (script_fetch("df_view", "height_adjust", &height_adjust, 1) || 
            height_adjust < 0 || height_adjust > (layer_config.height >> 2)) {
        height_adjust = HEIGHT_ADJUST;
    }
    db_msg("height adjust: %d\n", height_adjust);

    if (script_fetch("df_view", "menu_bgcolor", &menu_bgcolor, 1) || 
            menu_bgcolor < COLOR_WHITE_IDX || menu_bgcolor > COLOR_BLACK_IDX) {
        menu_bgcolor = COLOR_YELLOW_IDX;
    }

    if (script_fetch("df_view", "menu_fgcolor", &menu_fgcolor, 1) || 
            menu_fgcolor < COLOR_WHITE_IDX || menu_fgcolor > COLOR_BLACK_IDX) {
        menu_fgcolor = COLOR_BLACK_IDX;
    }

    if (script_fetch("df_view", "item_init_bgcolor", &item_init_bgcolor, 1) ||
            item_init_bgcolor < COLOR_WHITE_IDX || 
            item_init_bgcolor > COLOR_BLACK_IDX) {
        item_init_bgcolor = COLOR_WHITE_IDX;
    }

    if (script_fetch("df_view", "item_init_fgcolor", &item_init_fgcolor, 1) || 
            item_init_fgcolor < COLOR_WHITE_IDX || 
            item_init_fgcolor > COLOR_BLACK_IDX) {
        item_init_fgcolor = COLOR_BLACK_IDX;
    }

    if (script_fetch("df_view", "item_ok_bgcolor", &item_ok_bgcolor, 1) || 
            item_ok_bgcolor < COLOR_WHITE_IDX || 
            item_ok_bgcolor > COLOR_BLACK_IDX) {
        item_ok_bgcolor = COLOR_WHITE_IDX;
    }

    if (script_fetch("df_view", "item_ok_fgcolor", &item_ok_fgcolor, 1) || 
            item_ok_fgcolor < COLOR_WHITE_IDX || 
            item_ok_fgcolor > COLOR_BLACK_IDX) {
        item_ok_fgcolor = COLOR_BLUE_IDX;
    }

    if (script_fetch("df_view", "item_fail_bgcolor", &item_fail_bgcolor, 1) || 
            item_fail_bgcolor < COLOR_WHITE_IDX || 
            item_fail_bgcolor > COLOR_BLACK_IDX) {
        item_fail_bgcolor = COLOR_WHITE_IDX;
    }

    if (script_fetch("df_view", "item_fail_fgcolor", &item_fail_fgcolor, 1) || 
            item_fail_fgcolor < COLOR_WHITE_IDX || 
            item_fail_fgcolor > COLOR_BLACK_IDX) {
        item_fail_fgcolor = COLOR_RED_IDX;
    }

    if (script_fetch("df_view", "pass_str", (int *)pass_str, 
                sizeof(pass_str) / 4 - 1)) {
        strcpy(pass_str, "Pass");
    }

    if (script_fetch("df_view", "fail_str", (int *)fail_str, 
                sizeof(fail_str) / 4 - 1)) {
        strcpy(fail_str, "Fail");
    }

    return 0;
}

static int df_windows_init(void)
{
    unsigned int args[4];
    int disp;
    int fb;
    unsigned int layer_id;
    int argc;
    char **argv;

    /* open /dev/disp */
    if ((disp = open("/dev/disp", O_RDWR)) == -1) {
        db_error("can't open /dev/disp(%s)\n", strerror(errno));
        return -1;
    }

    /* open /dev/fb0 */
    fb = open("/dev/fb0", O_RDWR);
    if (ioctl(fb, FBIOGET_LAYER_HDL_0, &layer_id) < 0) {
        db_error("can't open /dev/fb0(%s)\n", strerror(errno));
        close(disp);
        return -1;
    }

    /* set layer top */
    args[0] = 0;
    args[1] = layer_id;
    ioctl(disp, DISP_CMD_LAYER_BOTTOM, args);

    close(disp);
    close(fb);

    /* init directfb */
    argc = 1;
    argv = malloc(sizeof(char *) * argc);
    argv[0] = "df_view";
    DFBCHECK(DirectFBInit(&argc, &argv));
    DFBCHECK(DirectFBCreate(&dfb));

    dfb->GetDeviceDescription(dfb, &gdesc);

    DFBCHECK(dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &layer));

    layer->SetCooperativeLevel(layer, DLSCL_ADMINISTRATIVE);

    if (!((gdesc.blitting_flags & DSBLIT_BLEND_ALPHACHANNEL) && 
                (gdesc.blitting_flags & DSBLIT_BLEND_COLORALPHA))) {
        layer_config.flags = DLCONF_BUFFERMODE;
        layer_config.buffermode = DLBM_BACKSYSTEM;

        layer->SetConfiguration(layer, &layer_config);
    }

    layer->GetConfiguration(layer, &layer_config);
    layer->EnableCursor(layer, 1);

    db_msg("screen_width: %d, sceen_height: %d\n", layer_config.width, layer_config.height);

    /* init font */
    df_font_init();

    /* init config */
    df_config_init();

    auto_tc_window_init();
    manual_tc_window_init();
    video_window_init();
    misc_window_init();

    tp_track_init();

    INIT_LIST_HEAD(&auto_tc_list);
    INIT_LIST_HEAD(&manual_tc_list);

    return 0;
}

static int df_insert_item(int id, struct item_data *data)
{
    struct list_head *pos, *tc_list;
    IDirectFBSurface *surface;
    int window_height;
    int x, y;
    struct df_item_data *df_data;
    char item_str[132];

    if (data == NULL)
        return -1;

    if (data->category == CATEGORY_AUTO) {
        tc_list = &auto_tc_list;
        surface = auto_tc_surface;
        window_height = auto_tc_window_height;
    }
    else if (data->category == CATEGORY_MANUAL) {
        tc_list = &manual_tc_list;
        surface = manual_tc_surface;
        window_height = manual_tc_window_height;
    }
    else {
        db_error("unknown category of item: %s\n", data->name);
        return -1;
    }

    x = 0;
    y = MENU_HEIGHT;
    list_for_each(pos, tc_list) {
        struct df_item_data *temp = list_entry(pos, struct df_item_data, list);
        y += temp->height;
    }

    if (y + ITEM_HEIGHT > window_height) {
        db_error("no more space for item: %s\n", data->name);
        return -1;
    }

    df_data = malloc(sizeof(struct df_item_data));
    if (df_data == NULL)
        return -1;

    df_data->id = id;
    strncpy(df_data->name, data->name, 32);
    strncpy(df_data->display_name, data->display_name, 64);
    df_data->category = data->category;
    df_data->status = data->status;
    if (data->exdata[0]) {
        strncpy(df_data->exdata, data->exdata, 64);
        snprintf(item_str, 128, "%s %s", df_data->display_name, df_data->exdata);
    }
    else {
        snprintf(item_str, 128, "%s", df_data->display_name);
    }

    df_data->x      = x;
    df_data->y      = y;
    df_data->width  = auto_tc_window_width;
    df_data->height = ITEM_HEIGHT;

    df_data->bgcolor = item_init_bgcolor;
    df_data->fgcolor = item_init_fgcolor;

#if 0
    db_debug("test case: %s\n", df_data->name);
    db_debug("        x: %d\n", df_data->x);
    db_debug("        y: %d\n", df_data->y);
    db_debug("    width: %d\n", df_data->width);
    db_debug("   height: %d\n", df_data->height);
#endif

    list_add(&df_data->list, tc_list);

    DFBCHECK(surface->SetColor(surface, 
                               color_tables[df_data->bgcolor].r, 
                               color_tables[df_data->bgcolor].g, 
                               color_tables[df_data->bgcolor].b, 0xff));
    DFBCHECK(surface->FillRectangle(surface, df_data->x, df_data->y, df_data->width, df_data->height));
    DFBCHECK(surface->SetFont(surface, font));
    DFBCHECK(surface->SetColor(surface, 
                               color_tables[df_data->fgcolor].r, 
                               color_tables[df_data->fgcolor].g, 
                               color_tables[df_data->fgcolor].b, 0xff));
    DFBCHECK(surface->DrawString(surface, item_str, -1, df_data->x + 4, 
                df_data->y + df_data->height - font_size / 4, DSTF_LEFT));

    surface->Flip(surface, NULL, 0);

    return 0;
}

static int df_update_item(int id, struct item_data *data)
{
    struct list_head *pos, *tc_list;
    IDirectFBSurface *surface;
    struct df_item_data *df_data;
    char item_str[132];

    if (data == NULL)
        return -1;

    if (data->category == CATEGORY_AUTO) {
        tc_list = &auto_tc_list;
        surface = auto_tc_surface;
    }
    else if (data->category == CATEGORY_MANUAL) {
        tc_list = &manual_tc_list;
        surface = manual_tc_surface;
    }
    else {
        db_error("unknown category of item: %s\n", data->name);
        return -1;
    }

    df_data = NULL;
    list_for_each(pos, tc_list) {
        struct df_item_data *temp = list_entry(pos, struct df_item_data, list);
        if (temp->id == id) {
            df_data = temp;
            break;
        }
    }

    if (df_data == NULL) {
        db_error("no such test case id #%d, name: %s\n", id, data->name);
        return -1;
    }

    df_data->status = data->status;
    if (df_data->status < 0) {
        df_data->bgcolor = item_init_bgcolor;
        df_data->fgcolor = item_init_fgcolor;
    }
    else if (df_data->status == 0) {
        df_data->bgcolor = item_ok_bgcolor;
        df_data->fgcolor = item_ok_fgcolor;
    }
    else {
        df_data->bgcolor = item_fail_bgcolor;
        df_data->fgcolor = item_fail_fgcolor;
    }

    if (data->exdata[0]) {
        strncpy(df_data->exdata, data->exdata, 64);
        if (df_data->status < 0) {
            snprintf(item_str, 128, "%s: %s", df_data->display_name, 
                                              df_data->exdata);
        }
        else if (df_data->status == 0) {
            snprintf(item_str, 128, "%s: [%s] %s", df_data->display_name, 
                                                   pass_str,
                                                   df_data->exdata);
        }
        else {
            snprintf(item_str, 128, "%s: [%s] %s", df_data->display_name, 
                                                   fail_str,
                                                   df_data->exdata);
        }
    }
    else {
        if (df_data->status < 0) {
            snprintf(item_str, 128, "%s", df_data->display_name);
        }
        else if (df_data->status == 0) {
            snprintf(item_str, 128, "%s: [%s]", df_data->display_name, 
                                                pass_str);
        }
        else {
            snprintf(item_str, 128, "%s: [%s]", df_data->display_name, 
                                                fail_str);
        }
    }

#if 0
    db_debug("test case: %s\n", df_data->name);
    db_debug("        x: %d\n", df_data->x);
    db_debug("        y: %d\n", df_data->y);
    db_debug("    width: %d\n", df_data->width);
    db_debug("   height: %d\n", df_data->height);
#endif

    DFBCHECK(surface->SetColor(surface, 
                               color_tables[df_data->bgcolor].r, 
                               color_tables[df_data->bgcolor].g, 
                               color_tables[df_data->bgcolor].b, 0xff));
    DFBCHECK(surface->FillRectangle(surface, df_data->x, df_data->y, df_data->width, df_data->height));
    DFBCHECK(surface->SetFont(surface, font));
    DFBCHECK(surface->SetColor(surface, 
                               color_tables[df_data->fgcolor].r, 
                               color_tables[df_data->fgcolor].g, 
                               color_tables[df_data->fgcolor].b, 0xff));
    DFBCHECK(surface->DrawString(surface, item_str, -1, df_data->x + 4, 
                df_data->y + df_data->height - font_size / 4, DSTF_LEFT));

    surface->Flip(surface, NULL, 0);

    return 0;

}

static int df_delete_item(int id)
{
    return -1;
}

static void df_sync(void)
{
    auto_tc_surface->Flip(auto_tc_surface, NULL, 0);
    manual_tc_surface->Flip(manual_tc_surface, NULL, 0);
}

static struct view_operations df_view_ops = 
{
    .insert_item = df_insert_item,
    .update_item = df_update_item,
    .delete_item = df_delete_item,
    .sync        = df_sync
};

static void button_handle(int x, int y)
{
    static int press = 0;

    /* check clear button is press */
    if (x > btn_rect.x && x < btn_rect.x + btn_rect.width &&
        y > btn_rect.y && y < btn_rect.y + btn_rect.height) {
        if (press) {
            press = 0;
            tp_track_clear();
        }
        else {
            press = 1;
        }
    }
    else {
        press = 0;
    }
}

static void show_mouse_event(DFBInputEvent *evt)
{
    static int press = 0;
    static int tp_x = -1, tp_y = -1;
    static int flag = 0x00;

    int mouse_x = 0, mouse_y = 0;

    char buf[64];

    *buf = 0;
    if (evt->type == DIET_AXISMOTION) {
        if (evt->flags & DIEF_AXISABS) {
            switch (evt->axis) {
                case DIAI_X:
                    mouse_x = evt->axisabs;
                    break;
                case DIAI_Y:
                    mouse_y = evt->axisabs;
                    break;
                case DIAI_Z:
                    snprintf(buf, sizeof(buf), "Z axis (abs): %d", evt->axisabs);
                    break;
                default:
                    snprintf(buf, sizeof(buf), "Axis %d (abs): %d", evt->axis, evt->axisabs);
                    break;
            }
        }
        else if (evt->flags & DIEF_AXISREL) {
            switch (evt->axis) {
                case DIAI_X:
                    mouse_x += evt->axisrel;
                    break;
                case DIAI_Y:
                    mouse_y += evt->axisrel;
                    break;
                case DIAI_Z:
                    snprintf(buf, sizeof(buf), "Z axis (rel): %d", evt->axisrel);
                    break;
                default:
                    snprintf (buf, sizeof(buf), "Axis %d (rel): %d", evt->axis, evt->axisrel);
                    break;
            }
        }
        else {
            db_debug("axis: %x\n", evt->axis);
        }

        mouse_x = CLAMP(mouse_x, 0, layer_config.width  - 1);
        mouse_y = CLAMP(mouse_y, 0, layer_config.height - 1);
    }
    else {
         snprintf(buf, sizeof(buf), "Button %d", evt->button);
    }

    db_dump("x #%d, y #%d\n", mouse_x, mouse_y);
    if (buf[0]) {
        db_dump("mouse event: %s\n", buf);
    }

    if (mouse_x != 0) {
        flag |= 0x01;
        tp_x = mouse_x;
    }
    if (mouse_y != 0) {
        flag |= 0x02;
        tp_y = mouse_y;
    }

    /* handle button event */
    if (mouse_x == 0 && mouse_y == 0) {
        tp_data.status = 0;
        snprintf(tp_data.exdata, 64, "(%d, %d)", tp_x, tp_y);
        df_update_item(BUILDIN_TC_ID_TP, &tp_data);
        button_handle(tp_x, tp_y);

        if (press) {
            /* draw last point anyway */
            tp_track_draw(tp_x, tp_y, -1);
            flag &= ~0x03;
            press = 0;
        }
        else {
            /* draw first point if we have get x and y */
            if ((flag & 0x03) == 0x03) {
                tp_track_draw(tp_x, tp_y, press);
            }
            flag &= ~0x03;
            press = 1;
        }
    }
    else {
        /* draw this point if we have get x and y */
        if ((flag & 0x03) == 0x03) {
            tp_track_draw(tp_x, tp_y, press);
            flag &= ~0x03;
        }
    }
}

/*
 * event main loop
 */
static void *event_mainloop(void *args)
{
    DFBInputDeviceKeySymbol last_symbol = DIKS_NULL;

    while (1) {
        DFBInputEvent evt;

        DFBCHECK(events->WaitForEvent(events));
        while (events->GetEvent(events, DFB_EVENT(&evt)) == DFB_OK) {
            show_mouse_event(&evt);
        }

        if (evt.type == DIET_KEYRELEASE) {
            if ((last_symbol == DIKS_ESCAPE || last_symbol == DIKS_EXIT) &&
                    (evt.key_symbol == DIKS_ESCAPE || evt.key_symbol == DIKS_EXIT)) {
                db_debug("Exit event main loop...\n");
                break;
            }

            last_symbol = evt.key_symbol;
        }
    }

    return (void *)0;
}

static int buildin_tc_init(void)
{
    char name[32];
    char display_name[64];

    memset(&tp_data, 0, sizeof(struct item_data));
    strncpy(name, "tp", 32);
    if (script_fetch(name, "display_name", (int *)display_name, sizeof(display_name) / 4)) {
        strncpy(tp_data.display_name, TP_DISPLAY_NAME, 64);
    }
    else {
        strncpy(tp_data.display_name, display_name, 64);
    }

    tp_data.category = CATEGORY_MANUAL;
    tp_data.status   = -1;
    df_insert_item(BUILDIN_TC_ID_TP, &tp_data);

    return 0;
}

int df_view_init(void)
{
    int ret;

    db_msg("directfb view init...\n");
    df_windows_init();
    df_view_id = register_view("directfb", &df_view_ops);
    if (df_view_id == 0)
        return -1;

    buildin_tc_init();

    /* get touchscreen */
    DFBCHECK(dfb->GetInputDevice(dfb, 1, &tp));
    /* create an event buffer for touchscreen */
    DFBCHECK(tp->CreateEventBuffer(tp, &events));

    /* create event mainloop */
    ret = pthread_create(&evt_tid, NULL, event_mainloop, NULL);
    if (ret != 0) {
        db_error("create event mainloop failed\n");
        unregister_view(df_view_id);
        return -1;
    }

    return 0;
}

int df_view_exit(void)
{
    unregister_view(df_view_id);
    return 0;
}
