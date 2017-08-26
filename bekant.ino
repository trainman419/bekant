#include "lin.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <aREST.h>
#include <aREST_UI.h>

// Pin settings for Sparkfun ESP8266 Thing Dev
#define UP_BTN 0
#define DOWN_BTN 13
#define LED 5
#define LED_ON LOW
#define TX_PIN 1
#define RX_PIN 3

#else
// Standard arduino settings
#define UP_BTN 7
#define DOWN_BTN 8
#define LED 13
#define LED_ON HIGH
#define TX_PIN 1
#define RX_PIN 0
#endif

Lin lin(Serial, TX_PIN);

#ifdef ESP8266
aREST_UI rest = aREST_UI();

const char * ssid = "Linksays";

WiFiServer server(80);
#endif

int height = 0;

enum class Command {
  NONE,
  UP,
  DOWN,
};

Command user_cmd = Command::NONE;

int up(bool pushed) {
  if(pushed) {
    digitalWrite(LED, LED_ON);
    user_cmd = Command::UP;
  } else {
    digitalWrite(LED, !LED_ON);
    user_cmd = Command::NONE;
  }
}

int down(bool pushed) {
  if(pushed) {
    digitalWrite(LED, LED_ON);
    user_cmd = Command::DOWN;
  } else {
    digitalWrite(LED, !LED_ON);
    user_cmd = Command::NONE;
  }
}

#ifdef ESP8266
int restCallback(String answer) {
  return rest.buttonCallback(answer);
}
#endif

void setup() {
  // put your setup code here, to run once:
  lin.begin(19200);

  pinMode(UP_BTN, INPUT_PULLUP);
  pinMode(DOWN_BTN, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, !LED_ON);

#ifdef ESP8266
  rest.title("Desk");
  rest.variable("Height", &height);
  rest.label("Height");
  rest.set_name("esp8266");

  rest.buttonFunction("Up", &up);
  rest.buttonFunction("Down", &down);
  rest.function("ui_callback", &restCallback);

  WiFi.mode(WIFI_AP);

  //WiFi.softAP(ssid, password);
  WiFi.softAP(ssid);

  server.begin();
#endif
}

unsigned long t = 0;

void delay_until(unsigned long ms) {
  unsigned long end = t + (1000 * ms);
  unsigned long d = end - micros();

  // crazy long delay; probably negative wrap-around
  // just return
  if ( d > 1000000 ) {
    t = micros();
    return;
  }
  
  if (d > 15000) {
    unsigned long d2 = (d-15000)/1000;
    delay(d2);
    d = end - micros();
  }
  delayMicroseconds(d);
  t = end;
}

enum class State {
  OFF,
  STARTING,
  UP,
  DOWN,
  STOPPING1,
  STOPPING2,
};

State state = State::OFF;

void loop() {
  // put your main code here, to run repeatedly:
  uint8_t empty[] = { 0, 0, 0 };
  uint8_t node_a[4] = { 0, 0, 0, 0 };
  uint8_t node_b[4] = { 0, 0, 0, 0 };
  uint8_t cmd[3] = { 0, 0, 0 };
  uint8_t res = 0;


  // Send ID 11
  lin.send(0x11, empty, 3, 2);
  delay_until(5);

  // Recv from ID 08
  res = lin.recv(0x08, node_a, 3, 2);
  delay_until(5);

  // Recv from ID 09
  res = lin.recv(0x09, node_b, 3, 2);
  delay_until(5);

  // Send ID 10, 6 times
  for (uint8_t i = 0; i < 6; i++) {
    lin.send(0x10, 0, 0, 2);
    delay_until(5);
  }

  // Send ID 1
  lin.send(0x01, 0, 0, 2);
  delay_until(5);

  uint16_t enc_a = node_a[0] | (node_a[1] << 8);
  uint16_t enc_b = node_b[0] | (node_b[1] << 8);
  uint16_t enc_target = enc_a;
  height = enc_a;

  // Send ID 12
  switch (state) {
    case State::OFF:
      cmd[2] = 0xFC; // 0b11111100
      break;
    case State::STARTING:
      cmd[2] = 0xC4; // 0b10100100
      break;
    case State::UP:
      enc_target = min(enc_a, enc_b);
      cmd[2] = 0x86; // 0b10000110
      break;
    case State::DOWN:
      enc_target = max(enc_a, enc_b);
      cmd[2] = 0x85; // 0b10000101
      break;
    case State::STOPPING1:
      cmd[2] = 0x87; // 0b10000111
      break;
    case State::STOPPING2:
      cmd[2] = 0x84; // 0b10000100
      break;
  }
  cmd[0] = enc_target & 0xFF;
  cmd[1] = enc_target >> 8;
  lin.send(0x12, cmd, 3, 2);

  // read buttons and compute next state
#ifndef ESP8266
  if (digitalRead(UP_BTN) == 0) { // UP
    up(true);
  } else if (digitalRead(DOWN_BTN) == 0) { // DOWN
    down(true);
  } else {
    up(false);
  }
#endif

  switch (state) {
    case State::OFF:
      if (user_cmd != Command::NONE) {
        if ( node_a[2] == 0x60 && node_b[2] == 0x60) {
          state = State::STARTING;
        }
      }
      break;
    case State::STARTING:
      //if( node_a[2] == 0x02 && node_b[2] == 0x02) {
        switch(user_cmd) {
          case Command::NONE:
            state = State::OFF;
            break;
          case Command::UP:
            state = State::UP;
            break;
          case Command::DOWN:
            state = State::DOWN;
            break;
        }
      //}
      break;
    case State::UP:
      if (user_cmd != Command::UP) {
        state = State::STOPPING1;
      }
      break;
    case State::DOWN:
      if (user_cmd != Command::DOWN) {
        state = State::STOPPING1;
      }
      break;
    case State::STOPPING1:
      state = State::STOPPING2;
      break;
    case State::STOPPING2:
      if ( node_a[2] == 0x60 && node_b[2] == 0x60) {
        state = State::OFF;
      }
      break;
    default:
      state = State::OFF;
      break;
  }

#ifdef ESP8266
  // Use the remaining time to service clients
  WiFiClient client = server.available();
  if(client) {
    while(!client.available()) {
      delay(1);
    }
    rest.handle(client);
  }
#endif

  // Wait the remaining 150 ms in the cycle
  delay_until(150);
}
