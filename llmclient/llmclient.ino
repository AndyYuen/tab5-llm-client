#include <Arduino.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebSocketsClient.h> 
#include "ui.h"
#include "llmclient.h"

// *******************************************
// Arduino libraries version used:
// * lvgl 8.3.0
// * WebSockets 2.7.0
// * M5GFX 0.2.8
// * Arduinojson 7.4.2
//
// Boards Manager:
// * M5Stack 3.2.2
// 
// Arduino IDE:
// * arduino-ide_nightly-20250404_Linux_64bit
// *******************************************

// WIFI variables
const char* ssid = "my-ssid"; // replace with your SSID
const char* password = "my-password"; // replace with your password

WebSocketsClient client;

// simple functions to hide websocket client object from caller
// **************************************************
// connect to the specified websocket
void websocket_connect(const char *host, uint16_t port, const char * path) {
  // Connect to the server
  if (client.isConnected()) {
    websocket_disconnect();
  }
  client.begin(host, port, path);

}

// disconnect the websocket
void websocket_disconnect() {
  client.disconnect();
}

// send text via the websocket
void websocket_send_text(const char * text) {
  Serial.printf("Sending question: %s\n", text);
  client.sendTXT(text);
}
// **************************************************

// WebSocket event handler
void websocket_event_handler(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WebSocket] Disconnected!");
      // websocket_disconnect();  // just to make sure no auto reconnect
      handle_disconnection("Connect");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to url: %s\n", payload);
      handle_connection("Connect");
      break;
    case WStype_TEXT:
      // Serial.printf("[WebSocket] Received text: %s\n", payload);
      // process the received text here
      process_recevied_text(payload);
      break;
    case WStype_BIN:
      Serial.printf("[WebSocket] Received binary data of length %d\n", length);
      // You can process received binary data here
      break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      // Other event types can be handled here if needed
      break;
  }
}


void setup()
{
  // M5Unified initialises LCD display
  M5.begin();
  
  Serial.begin(115200);
  delay(10);

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA); // Set the board to Station mode

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // initialise diplay
  M5.Display.init();

  // start UI
  create_ui();

  M5.Display.setBrightness(128);

  client.disableHeartbeat();
  client.onEvent(websocket_event_handler);

  // Set reconnect interval if connection fails
  client.setReconnectInterval(10000); 
}

void loop() {
  // let LVGL do its processing)
  lv_timer_handler();

  client.loop();

  // set small delay to avoid busy-waiting
  delay(5); 
}

