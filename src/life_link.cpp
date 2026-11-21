#if defined(LIFE_TX) || defined(LIFE_RX)
#include <Arduino.h>
#include <SPI.h>
#include <SSD1306.h>
#include <RadioLib.h>
#include <esp_system.h>
#include "life_grid.h"

static SSD1306 oled(0x3c, 17, 18);
static SX1262 radio = new Module(8, 14, 12, 13);
static LifeGrid game;
static uint32_t lastStep, lastPacket;
static constexpr uint32_t tickMs = 100, interval = 10000;
static constexpr uint16_t roundLength = 500;
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
static void draw() {
  oled.clear();
  for (int y = 0; y < 64; ++y) for (int x = 0; x < 128; ++x)
    if (game.cells[y][x]) oled.setPixel(x, y);
  oled.display();
}
#ifdef LIFE_RX
static volatile bool received;
static uint16_t target;
static void IRAM_ATTR onReceive() { received = true; }
static void pollPacket() {
  if (!received) return;
  received = false;
  uint8_t p[10]{};
  size_t length = radio.getPacketLength();
  int status = radio.readData(p, sizeof p);
  check(radio.startReceive());
  if (status || length != sizeof p || memcmp(p, "GLV1", 4)) return;
  uint32_t seed = 0;
  for (int i = 4; i < 8; ++i) seed = (seed << 8) | p[i];
  uint16_t gen = (uint16_t(p[8]) << 8) | p[9];
  if (!seed || gen >= roundLength) return;
  if (seed != game.seed || game.generation > gen) game.reset(seed);
  target = gen;
  lastPacket = millis();
  lastStep = lastPacket;
}
#else
static void newGame() {
  uint32_t seed = esp_random();
  game.reset(seed ? seed : 1);
}
static void sendPacket() {
  uint8_t p[10] = {'G','L','V','1'};
  for (int i = 0; i < 4; ++i) p[4+i] = game.seed >> (24-8*i);
  p[8] = game.generation >> 8;
  p[9] = game.generation;
  check(radio.transmit(p, sizeof p));
  lastPacket = millis();
}
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
#ifdef LIFE_TX
  digitalWrite(5, HIGH);
#else
  digitalWrite(5, LOW);
#endif
  delay(20);
  SPI.begin(9, 11, 10, 8);
  check(radio.begin(868.3, 125.0, 7, 5, 0x12, -9, 8, 1.8));
  check(radio.setDio2AsRfSwitch(true));
#ifdef LIFE_TX
  newGame(); draw(); sendPacket();
#else
  message("RX wartet auf TX");
  radio.setPacketReceivedAction(onReceive);
  check(radio.startReceive());
#endif
  lastStep = millis();
}
void loop() {
#ifdef LIFE_RX
  pollPacket();
  if (!game.seed) { delay(1); return; }
  if (game.generation < target) {
    game.step();
    if (game.generation == target) draw();
    yield();
    return;
  }
#endif
  if (millis() - lastStep >= tickMs) {
    lastStep += tickMs;
#ifdef LIFE_TX
    if (game.generation + 1 >= roundLength) newGame();
    else game.step();
#else
    if (game.generation + 1 >= roundLength) { delay(1); return; }
    game.step();
#endif
    draw();
  }
#ifdef LIFE_TX
  if (millis() - lastPacket >= interval) sendPacket();
#endif
  delay(1);
}
#endif
