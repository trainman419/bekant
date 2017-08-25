#include "lin.h"

Lin lin;

void setup() {
  // put your setup code here, to run once:
  lin.begin(19200);
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
  cmd[2] = 0xFC;
  lin.send(0x12, cmd, 3, 2);

  // Wait the remaining 150 ms in the cycle
  digitalWrite(LED_BUILTIN, HIGH);
  delay_until(150);
}
