#ifndef _LLM_CLIENT_H
#define _LLM_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

// websocket utilities
void websocket_connect(const char *host, uint16_t port, const char * path);
void websocket_disconnect();
void websocket_send_text(const char * text);


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif