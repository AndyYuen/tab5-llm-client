#ifndef _UI_H
#define _UI_H

#include <lvgl.h>

// Tab5 LCD resolution
#define LCD_H_RES 720
#define LCD_V_RES 1280

#define LVGL_LCD_BUF_SIZE     (LCD_H_RES * LCD_V_RES)

// colors for textareas
#define HEX_COLOUR_RED    0xeb4934
#define HEX_COLOUR_GREEN  0x80eb34
#define HEX_COLOUR_BLUE   0x349ceb
#define HEX_COLOUR_ORANGE 0xeb9c34


#ifdef __cplusplus
extern "C" {
#endif

// create UI
void create_ui(void);

void disable_connect_btn();
void handle_disconnection(char *text);
void handle_connection(char *text);
void process_recevied_text(uint8_t * payload);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif