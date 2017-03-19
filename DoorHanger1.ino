#include "FastLED.h"

FASTLED_USING_NAMESPACE
#define DATA_PIN 4
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
CRGB strip[1];

// Shift register pins
#define SFT_LATCH A3
#define SFT_CLK A4
#define SFT_DATA A5

void setup() {
  Serial.begin(115200);
  
  // Strip lights
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(strip, 1).setCorrection(TypicalLEDStrip);
  // Set master brightness control
  FastLED.setBrightness(255);

  // Setup pin-connected lights
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A0, OUTPUT);
  pinMode(12, OUTPUT);

  // Setup PWM pin-connected lights
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);

  // Setup shift register pins
  pinMode(SFT_LATCH, OUTPUT);
  pinMode(SFT_DATA, OUTPUT);
  pinMode(SFT_CLK, OUTPUT);
}

byte shiftBuffer[2] = {0, 0};
void setShiftLight(byte light, byte state) {
  // Change buffer entry
  if (state) {
    shiftBuffer[light / 8] |= 1 << (light%8);
  } else {
    shiftBuffer[light / 8] &= ~(1 << (light%8));
  }

  // Write out change to shift registers
  shiftOut(SFT_DATA, SFT_CLK, MSBFIRST, shiftBuffer[0]);
  shiftOut(SFT_DATA, SFT_CLK, MSBFIRST, shiftBuffer[1]);
  digitalWrite(SFT_LATCH, HIGH);
  digitalWrite(SFT_LATCH, LOW);
}

const byte ZOMBIE_LIGHT_PINS[] = {11, 10, 9, 5, 6};
void setZombieLight(byte light, byte value) {
  analogWrite(ZOMBIE_LIGHT_PINS[light], value);
}

const byte PLAYER_LIGHT_SHIFT_PINS[] = {1, 2, 14, 8};
const byte PLAYER_LIGHT_PINS[] = {7, 8, A0, A1, 12};
void setPlayerLight(byte light, byte state) {
  if (light <= 3) {
    // Shift register
    setShiftLight(PLAYER_LIGHT_SHIFT_PINS[light], state);
  } else {
    // Direct pin
    digitalWrite(PLAYER_LIGHT_PINS[light-4], state);
  }
}

const byte MAGIC_RING_SHIFT_PINS[] = {9, 11, 12, 10};
void setMagicRingLight(byte light, byte state) {
  setShiftLight(MAGIC_RING_SHIFT_PINS[light], state);
}

const byte MAGIC_STUFF_SHIFT_PINS[] = {3, 4, 5};
void setMagicStuffLight(byte light, byte state) {
  setShiftLight(MAGIC_STUFF_SHIFT_PINS[light], state);
}

void show3BitsOnZombies(int i) {
  setZombieLight(0, 255 * ((i & 0x01) > 0));
  setZombieLight(1, 255 * ((i & 0x02) > 0));
  setZombieLight(2, 255 * ((i & 0x04) > 0));
}


const byte SECRET_PASSAGES_SHIFT_PINS[] = {7+8, 5+8};
void setSecretPassagesLight(byte light, byte state) {
  setShiftLight(SECRET_PASSAGES_SHIFT_PINS[light], state);
}

const byte SECRET_SWITCH_SHIFT_PIN = 0;
void setSecretSwitchLight(byte state) {
  setShiftLight(SECRET_SWITCH_SHIFT_PIN, state);
}

void loop() {
  for (int i=0;i<2;i++){
    setSecretSwitchLight(i);
    show3BitsOnZombies(i);
    delay(1000);
  }
}
