#include <Arduino.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <stdarg.h>
#include <ArduinoJson.h>
#include "ui.h"
#include "llmclient.h"

#define MAX_BUFFER  63
#define MAX_STATUS_BUFFER   127

// status messages
const char *STATUS_PREFIX       = "#eb9c34 Status: # ";
const char *MSG_NOT_CONNECTED   = "Not connected";
const char *MSG_CONNECTED       = "Connected";
const char *MSG_CONNECTING      = "Connecting..";
const char *MSG_CONNECTION_FAILED   = "Connection failed";
const char *MSG_URI_EMPTY       = "URI empty";
const char *MSG_MISSING_WS      = "URI missing ws://";
const char *MSG_MISSING_HOST    = "URI missing host";
const char *MSG_MISSING_PORT    = "URI missing port";
const char *MSG_MISSING_PATH    = "URI Missing path";

char host[MAX_BUFFER + 1];
char path[MAX_BUFFER +1];
char status_buffer[MAX_STATUS_BUFFER + 1];
uint16_t port;


// lvgl variables
static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;

// UI objects
lv_obj_t * screen;

lv_obj_t * host_ta;
lv_obj_t * question_ta;
lv_obj_t * answer_ta;

lv_obj_t * status_lb;
lv_obj_t * connect_btn_lb;

lv_obj_t * connect_btn;
lv_obj_t * clear_btn;
lv_obj_t * submit_btn;


// returns a formatted message in red
const char *status_msg_red(const char * msg) {
    sprintf(status_buffer, "%s #eb4934 %s. #", STATUS_PREFIX, msg);
    return (const char *) status_buffer;
}

// returns a formatted message in green
const char *status_msg_green(const char * msg) {
    sprintf(status_buffer, "%s #80eb34 %s. #", STATUS_PREFIX, msg);
    return (const char *) status_buffer;
}

// returns a formatted message in default colour
const char *status_msg_default(const char * msg) {
    sprintf(status_buffer, "%s %s.", STATUS_PREFIX, msg);
    return (const char *) status_buffer;
}

// syntex-ckeck and parse for host, port and path
const char * parse_ws_uri(String uri,  char *host, uint16_t *port, char *path) {
    // Serial.printf("parse_ws_uri: %s\n", uri.c_str());
    if (uri.length() == 0) return MSG_URI_EMPTY;
    uri.trim();
    String prefix = "ws://";
    if (uri.startsWith(prefix)) {
        uri.remove(0, prefix.length());
        if (uri.length() == 0) return MSG_MISSING_HOST;
        int pos;
        if ((pos = uri.indexOf(":")) < 0) {
            return MSG_MISSING_PORT;
        }

        // copy the IPAddress to host array
        strncpy(host, uri.substring(0, pos).c_str(), MAX_BUFFER);

        int separator_pos;
        if ((separator_pos = uri.indexOf("/", pos + 1)) < 0) {
            return MSG_MISSING_PATH;
        }

        // copy port number to port
        *port = (uint16_t) uri.substring(pos + 1, separator_pos).toInt();

        // copy path to path array
        strncpy(path, uri.substring(separator_pos).c_str(), MAX_BUFFER);
        Serial.printf("host=%s, port=%d, path=%s\n", host, *port, path);
    }
    else {
        return MSG_MISSING_WS;
    }

    return "";

}

// actions to be carried out during event handling

// action on submit clicked
void get_answer() {
    lv_obj_add_flag(submit_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(clear_btn, LV_OBJ_FLAG_HIDDEN);

    JsonDocument doc;
    doc["question"] = lv_textarea_get_text(question_ta);
    String jsonStr;
    serializeJson(doc, jsonStr); 
    websocket_send_text(jsonStr.c_str());
}

void disable_connect_btn() {
    lv_obj_add_flag(connect_btn, LV_OBJ_FLAG_HIDDEN);
    Serial.println("Hid Connect button");
    // lv_obj_add_state(connect_btn, LV_STATE_DISABLED);
}

// output connection status and unhide the Connect button
void handle_disconnection(char *text) {
    lv_label_set_text(connect_btn_lb, text);
    lv_obj_clear_flag(connect_btn, LV_OBJ_FLAG_HIDDEN);
    Serial.println("Connection failed");
    websocket_disconnect();
    lv_label_set_text(status_lb, status_msg_red(MSG_CONNECTION_FAILED));

    // hide clear and submit button until websocket connection successful
    lv_obj_add_flag(submit_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(clear_btn, LV_OBJ_FLAG_HIDDEN);
}

// output connection status and unhide the Connect button
void handle_connection(char *text) {
    // lv_obj_t * label = lv_label_create(connect_btn);
    lv_label_set_text(connect_btn_lb, text);
    lv_obj_clear_flag(connect_btn, LV_OBJ_FLAG_HIDDEN);
    Serial.println("Connection succeeded");
    lv_label_set_text(status_lb, status_msg_green(MSG_CONNECTED));

    // show clear and submit button now that websocket connection is made
    lv_obj_clear_flag(submit_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(clear_btn, LV_OBJ_FLAG_HIDDEN);
}

// process response returned by the LLM
void process_recevied_text(uint8_t * payload) {
    JsonDocument doc;
    String content;
    deserializeJson(doc, payload);
    for (JsonPair kv : doc.as<JsonObject>()) {
        String key = kv.key().c_str();
        const char * value = kv.value().as<const char *>();
        // Serial.printf("key=%s : value=%s\n", key, value);
        if (key == "chunk") {
            lv_textarea_add_text(answer_ta, value);

        } 
        else if (key == "status") {
            if (String(value) == "Answer complete.")  {
                lv_obj_clear_flag(clear_btn, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(submit_btn, LV_OBJ_FLAG_HIDDEN);
                Serial.println("Answer complete received.");          
            }
            lv_label_set_text(status_lb, status_msg_default(value)); 
        }
        else {
            Serial.printf("Receieved unexpected message with key: %s\n", key);
        }
    }
}

// button callback function
void btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    lv_obj_t * btn = lv_event_get_target(e); // Get the button that triggered the event

    if (btn == connect_btn) {
        // connect button processing
        String uri_str = lv_textarea_get_text(host_ta);
        // Serial.printf("ws-URI: %s\n", uri_str.c_str());

        const char * parse_error = parse_ws_uri(uri_str, host, &port, path);           
        if (strlen(parse_error) != 0) {
            Serial.printf("URI error: %s\n", parse_error);
            lv_label_set_text(status_lb, status_msg_red(parse_error));
            return;               
        }

        disable_connect_btn();
        lv_textarea_set_text(answer_ta, "");
        Serial.printf("Connecting to: ws://%s:%d%S\n", host, port, path);
        lv_obj_add_flag(connect_btn, LV_OBJ_FLAG_HIDDEN);

        websocket_connect(host, port, path);
        lv_label_set_text(status_lb, status_msg_default(MSG_CONNECTING));
        Serial.println("websocket_connect called.");
    } else if (btn == clear_btn) {
        // clear button processing
        lv_textarea_set_text(question_ta, "");
        lv_textarea_set_text(answer_ta, ""); 
    } else if (btn == submit_btn ) {
        // submit button processing
        lv_textarea_set_text(answer_ta, "");
        lv_obj_add_flag(clear_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(submit_btn, LV_OBJ_FLAG_HIDDEN);

        JsonDocument doc;
        doc["question"] = lv_textarea_get_text(question_ta);
        String json_text;
        serializeJson(doc, json_text);
        // Serial.printf("Sending: %s\n", json_text.c_str());
        websocket_send_text(json_text.c_str());
    }

}

// textarea callback function
void ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    lv_obj_t * kb = (lv_obj_t *) lv_event_get_user_data(e);
    if(code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }

    if(code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

// callback to update changes in display
void lv_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    M5.Display.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full); 
    lv_disp_flush_ready(disp);
}

// touch callback
static void lv_indev_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{  
     lgfx::touch_point_t tp[3];
     uint8_t touchpad = M5.Display.getTouchRaw(tp,3);
       if (touchpad > 0)
       {
          data->state = LV_INDEV_STATE_PR;
          data->point.x = tp[0].x;
          data->point.y = tp[0].y;
          //Serial.printf("X: %d   Y: %d\n", tp[0].x, tp[0].y); //for testing
       }
       else
       {
        data->state = LV_INDEV_STATE_REL;
       }
}


// Layout the UI with widgets
void create_widgets(void)
{
    // set the lvgl theme
    lv_disp_t * dispp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), 
        lv_palette_main(LV_PALETTE_RED),
        true, LV_FONT_DEFAULT);       
    lv_disp_set_theme(dispp, theme);

    screen = lv_obj_create(NULL);

    // ceate a col1 flex container 
    lv_obj_t * cont_col1 = lv_obj_create(screen);
    lv_obj_set_size(cont_col1, 630, 550);
    lv_obj_align(cont_col1, LV_ALIGN_TOP_LEFT, 4, 5);
    lv_obj_set_flex_flow(cont_col1, LV_FLEX_FLOW_COLUMN);

    // ceate a col2 flex container 
    lv_obj_t * cont_col2 = lv_obj_create(screen);
    lv_obj_set_size(cont_col2, 630, 700);
    lv_obj_align(cont_col2, LV_ALIGN_TOP_LEFT, 640, 5);
    lv_obj_set_flex_flow(cont_col2, LV_FLEX_FLOW_COLUMN);

    // ceate a row1 flex container inside cont_col1
    lv_obj_t * cont_row1_in_col1 = lv_obj_create(cont_col1);
    lv_obj_set_size(cont_row1_in_col1, 620, 80);
    lv_obj_align(cont_row1_in_col1, LV_ALIGN_TOP_LEFT, 2, 5);
    lv_obj_set_flex_flow(cont_row1_in_col1, LV_FLEX_FLOW_ROW);

    // ceate a row1 flex container inside cont_col2
    lv_obj_t * cont_row1_in_col2 = lv_obj_create(cont_col2);
    lv_obj_set_size(cont_row1_in_col2, 620, 80);
    lv_obj_align(cont_row1_in_col2, LV_ALIGN_TOP_LEFT, 2, 5);
    lv_obj_set_flex_flow(cont_row1_in_col2, LV_FLEX_FLOW_ROW);

    // set font
    lv_style_t my_font_style;
    lv_style_init(&my_font_style);
    lv_style_set_text_font(&my_font_style, &lv_font_montserrat_22);

    // column 1 and screen widgets ****************************
    //
    // create a keyboard 
    lv_obj_t * kb = lv_keyboard_create(screen);
    lv_obj_set_width(kb, 890);
    lv_obj_set_height(kb, 300);

    // LLM host label
    // lv_obj_t * host_lb = lv_label_create(cont_row1_in_col1);
    // lv_obj_add_style(host_lb, &my_font_style, 0);  
    // lv_obj_set_style_text_color(host_lb, lv_color_hex(HEX_COLOUR_ORANGE), LV_PART_MAIN | LV_STATE_DEFAULT);        
    // lv_label_set_text(host_lb, "ws-URI: ");
    // lv_obj_align(host_lb, LV_ALIGN_TOP_LEFT, 2, 5);
    // lv_obj_set_width(host_lb, lv_pct(10));  
    // lv_obj_set_style_text_align(host_lb, LV_TEXT_ALIGN_LEFT, 1);

    // LLM host input. The keyboard will write here.
    host_ta = lv_textarea_create(cont_row1_in_col1);
    // lv_obj_align(host_ta, LV_ALIGN_TOP_LEFT, 55, 5);
    lv_obj_add_event_cb(host_ta, ta_event_cb, LV_EVENT_ALL, kb);
    // lv_textarea_set_one_line(host_ta, true);
    lv_obj_set_width(host_ta, lv_pct(60));
    // lv_textarea_set_placeholder_text(host_ta, "ws://IPAddress:port/ws");
    lv_textarea_set_text(host_ta, "ws://192.168.1.146:8080/ws");
    lv_keyboard_set_textarea(kb, host_ta);

    // host connect button
    connect_btn = lv_btn_create(cont_row1_in_col1);
    // lv_obj_align(connect_btn, LV_ALIGN_TOP_LEFT, 480, 5);
    connect_btn_lb = lv_label_create(connect_btn);
    lv_label_set_text(connect_btn_lb, "Connect");
    lv_obj_set_width(connect_btn, 130);
    lv_obj_add_event_cb(connect_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
    // lv_obj_center(connect_btn);

    question_ta = lv_textarea_create(cont_col1);
    lv_obj_set_style_text_color(question_ta, lv_color_hex(HEX_COLOUR_GREEN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(question_ta, LV_ALIGN_TOP_LEFT, 2, 10);
    lv_obj_add_event_cb(question_ta, ta_event_cb, LV_EVENT_ALL, kb);
    lv_textarea_set_placeholder_text(question_ta, "Enter you question for the LLM here.\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12");
    lv_obj_set_size(question_ta, 580, 300);

    // Clear and Submit buttons
    clear_btn = lv_btn_create(cont_col1);
    lv_obj_t * clear_btn_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_btn_label, "Clear");
    lv_obj_add_event_cb(clear_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(clear_btn_label);

    submit_btn = lv_btn_create(cont_col1);
    lv_obj_t * submit_button_label = lv_label_create(submit_btn);
    lv_label_set_text(submit_button_label, "Submit");
    lv_obj_add_event_cb(submit_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(submit_button_label);

    //column 2 widgets ****************************
    // 
    // status message
    status_lb = lv_label_create(cont_row1_in_col2);
    lv_obj_add_style(status_lb, &my_font_style, 0);  
    lv_label_set_recolor(status_lb, true);
    lv_label_set_text(status_lb, status_msg_red(MSG_NOT_CONNECTED));
    // lv_obj_set_style_text_align(status_lb, LV_TEXT_ALIGN_LEFT, 1);
    lv_obj_align(status_lb, LV_ALIGN_TOP_LEFT, 2, 5);

    // LLM answer on right column
    answer_ta = lv_textarea_create(cont_col2);
    lv_obj_set_style_text_color(answer_ta, lv_color_hex(HEX_COLOUR_BLUE), LV_PART_MAIN | LV_STATE_DEFAULT);         
    lv_obj_set_size(answer_ta, 580, 660); 
    lv_textarea_set_text(answer_ta, "Answer to your question appears here.\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n");
    // Make the text area read-only
    // Disable editing by not allowing input
    lv_obj_add_flag(answer_ta, LV_OBJ_FLAG_CLICKABLE); // Allow clicks for selection if desired, but not for input
    lv_obj_clear_flag(answer_ta, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE); // Prevent focus/press for input
    lv_textarea_set_cursor_click_pos(answer_ta, false); // Disable cursor positioning by clicking

    lv_disp_load_scr(screen);
}

// create the UI for the application
void create_ui() {
    // initialise lvgl
    lv_init();

    // set up lvgl display buffer in PSRAM
    // the M5 display has its own display memory
    buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE, 
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);
    
    // set up display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;

    disp_drv.flush_cb = lv_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.sw_rotate = 1;  
    disp_drv.rotated = LV_DISP_ROT_90; 
    lv_disp_drv_register(&disp_drv);

    // initialise touch
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lv_indev_read;
    lv_indev_drv_register(&indev_drv);

    // create and layout the widgets
    create_widgets();

    // hide clear and submit button until websocket connection is made
    lv_obj_add_flag(submit_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(clear_btn, LV_OBJ_FLAG_HIDDEN);
}
