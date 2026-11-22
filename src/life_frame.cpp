#if defined(LIFE_FRAME_TX) || defined(LIFE_FRAME_RX)
#include <Arduino.h>
#include <SPI.h>
#include <SSD1306.h>
#include <RadioLib.h>
#include <esp_system.h>
#include "life_bitmap.h"
#ifdef LIFE_FRAME_TX
#include "life_grid.h"
#endif

static SSD1306 oled(0x3c, 17, 18);
static SX1262 radio = new Module(8, 14, 12, 13);
static void message(const String &s) {
  oled.clear();
  oled.drawString(0, 24, s);
  oled.display();
}
static void check(int result) {
  if (result == RADIOLIB_ERR_NONE) return;
  message("Radio Fehler " + String(result));
  while (true) delay(500);
}
static uint8_t packet[bitmap::packetBytes];
static void displayImage(const bitmap::Image &image) {
  oled.clear();
  for (int y = 0; y < bitmap::height; ++y)
    for (int x = 0; x < bitmap::width; ++x)
      if (image[(y / 8) * bitmap::width + x] & (1U << (y % 8))) oled.setPixel(x, y);
  oled.display();
}
#ifdef LIFE_FRAME_TX
static LifeGrid game;
static bitmap::Image image;
static uint32_t frame;
static void startGame() {
  uint32_t seed = esp_random();
  game.reset(seed ? seed : 1);
}
static void broadcast() {
  memset(image, 0, sizeof image);
  for (int y = 0; y < bitmap::height; ++y)
    for (int x = 0; x < bitmap::width; ++x)
      if (game.cells[y][x]) image[(y / 8) * bitmap::width + x] |= 1U << (y % 8);
  displayImage(image);
  for (uint8_t part = 0; part < bitmap::pieces; ++part) {
    bitmap::encode(packet, image, frame, part);
    check(radio.transmit(packet, sizeof packet));
    delay(25);
  }
  ++frame;
}
#else
static bitmap::Receiver receiver;
static volatile bool received;
static void IRAM_ATTR onReceive() { received = true; }
#endif
void setup() {
  pinMode(36, OUTPUT); digitalWrite(36, LOW);
  delay(50);
  pinMode(21, OUTPUT); digitalWrite(21, LOW);
  delay(10); digitalWrite(21, HIGH); delay(50);
  oled.init(); oled.flipScreenVertically();
  pinMode(7, OUTPUT); digitalWrite(7, HIGH);
  pinMode(2, OUTPUT); digitalWrite(2, HIGH);
  pinMode(5, OUTPUT);
#ifdef LIFE_FRAME_TX
  digitalWrite(5, HIGH);
#else
  digitalWrite(5, LOW);
#endif
  delay(20);
  SPI.begin(9, 11, 10, 8);
  check(radio.begin(869.525, 125.0, 7, 5, 0x12, -9, 8, 1.8));
  check(radio.setDio2AsRfSwitch(true));
#ifdef LIFE_FRAME_TX
  frame = esp_random();
  startGame();
#else
  message("RX wartet auf Bild");
  radio.setPacketReceivedAction(onReceive);
  check(radio.startReceive());
#endif
}
void loop() {
#ifdef LIFE_FRAME_TX
  uint32_t started = millis();
  broadcast();
  while (millis() - started < 25000U) delay(10);
  if (game.generation >= 499) startGame();
  else game.step();
#else
  if (!received) { delay(1); return; }
  received = false;
  size_t length = radio.getPacketLength();
  int result = radio.readData(packet, sizeof packet);
  check(radio.startReceive());
  if (result == RADIOLIB_ERR_NONE && receiver.accept(packet, length))
    displayImage(receiver.pending);
#endif
}
#endif
