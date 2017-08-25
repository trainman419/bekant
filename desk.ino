#include "lin.h"

Lin lin;

void setup() {
  // put your setup code here, to run once:
  lin.begin(19200);

  pinMode(7, INPUT);
  digitalWrite(7, HIGH);
  pinMode(8, INPUT);
  digitalWrite(8, HIGH);
}

unsigned long t = 0;

void delay_until(unsigned long ms) {
  long end = t + (1000 * ms);
  long d = end - micros();

  // crazy long delay; probably negative wrap-around
  // just return
  if ( d > 1000000 ) {
    t = micros();
    return;
  }
  
  while(d > 1000) {
    delay(1);
    d -= 1000;
  }
  d = end - micros();
  delayMicroseconds(d);
  t = end;
}

enum class Command {
  NONE,
  UP,
  DOWN,
};

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
  digitalWrite(LED_BUILTIN, HIGH);
  // put your main code here, to run repeatedly:
  uint8_t empty[] = { 0, 0, 0 };
  uint8_t node_a[4] = { 0, 0, 0, 0 };
  uint8_t node_b[4] = { 0, 0, 0, 0 };
  uint8_t cmd[3] = { 0, 0, 0 };

  // Send ID 11
  lin.send(0x11, empty, 3, 2);
  delay_until(5);

  // Recv from ID 08
  lin.recv(0x08, node_a, 3, 2);
  delay_until(5);

  // Recv from ID 09
  lin.recv(0x09, node_b, 3, 2);
  delay_until(5);

  // Send ID 10, 6 times
  for(uint8_t i=0; i<6; i++) {
    lin.send(0x10, 0, 0, 2);
    delay_until(5);
  }

  // Send ID 1
  lin.send(0x01, 0, 0, 2);
  delay_until(5);

  // Send ID 12
  cmd[0] = node_a[0];
  cmd[1] = node_b[1];
  switch(state) {
    case State::OFF:
      cmd[2] = 0xFC;
      break;
    case State::STARTING:
      cmd[2] = 0x86;
      break;
    case State::UP:
      cmd[2] = 0x87;
      break;
    case State::DOWN:
      cmd[2] = 0x85;
      break;
    case State::STOPPING1:
      cmd[2] = 0x87;
      break;
    case State::STOPPING2:
      cmd[2] = 0x84;
      break;
  }
  cmd[2] = 0xFC;
  lin.send(0x12, cmd, 3, 2);

  // read buttons and compute next state
  Command user_cmd = Command::NONE;
  if(digitalRead(7) == 0) { // UP
    user_cmd = Command::UP;
  } else if(digitalRead(8) == 0) { // DOWN
    user_cmd = Command::DOWN;
  }

  switch(state) {
    case State::OFF:
      if(user_cmd != Command::NONE) {
        if( node_a[2] == 0x60 && node_b[2] == 0x60) {
          state = State::STARTING;
        }
      }
      break;
    case State::STARTING:
      if( node_a[2] == 0x02 && node_b[2] == 0x02) {
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
      }
      break;
    case State::UP:
      if(user_cmd != Command::UP) {
        state = State::STOPPING1;
      }
      break;
    case State::DOWN:
      if(user_cmd != Command::DOWN) {
        state = State::STOPPING1;
      }
      break;
    case State::STOPPING1:
      state = State::STOPPING2;
      break;
    case State::STOPPING2:
      if( node_a[2] == 0x60 && node_b[2] == 0x60) {
        state = State::OFF;
      }
      break;
  }

  // Wait the remaining 150 ms in the cycle
  digitalWrite(LED_BUILTIN, HIGH);
  delay_until(150);
}
