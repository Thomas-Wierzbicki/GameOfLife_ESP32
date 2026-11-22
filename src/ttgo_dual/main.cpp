#if defined(TTGO_DUAL_TX) || defined(TTGO_DUAL_RX)
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <SSD1306.h>
#include <RadioLib.h>
#include <esp_system.h>
#include "life_bitmap.h"
#ifdef TTGO_DUAL_TX
#include "life_grid.h"
#endif

static SSD1306 oled(0x3c, 21, 22);
static SX1276 radio = new Module(18, 26, 23, RADIOLIB_NC);
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
#ifdef TTGO_DUAL_TX
static LifeGrid game;
static bitmap::Image image;
static uint32_t frame;
static void startGame() {
  uint32_t seed = esp_random();
  game.reset(seed ? seed : 1);
}
static void prepareImage() {
  memset(image, 0, sizeof image);
  for (int y = 0; y < bitmap::height; ++y)
    for (int x = 0; x < bitmap::width; ++x)
      if (game.cells[y][x]) image[(y / 8) * bitmap::width + x] |= 1U << (y % 8);
  displayImage(image);

}
#else
static bitmap::Receiver receiver;
static volatile bool received;
static void IRAM_ATTR onReceive() { received = true; }
#endif
constexpr char ssid[] = "Life-H1";
constexpr char password[] = "HeltecLife43";
constexpr uint16_t port = 4210;
static WiFiUDP udp;
static bool wlan, bound;
static uint32_t lastFrame, lastLoRa;
static bool sentLoRa;
static volatile bool buttonEdge;
static uint32_t lastButton;
static void IRAM_ATTR buttonDown() { buttonEdge = true; }
struct WifiFrame {
  char magic[4];
  bitmap::Image pixels;
};
static WifiFrame wireFrame = {{'G','L','W','1'}, {}};
static_assert(sizeof(WifiFrame) == 1028);
#ifdef TTGO_DUAL_TX
static IPAddress peer;
static bool hasPeer;
static uint32_t lastHello;
static uint8_t part = bitmap::pieces;
#else
static uint32_t lastHello, lastImage;
static bool online, showingImage;
#endif

static void switchMode() {
  wlan = !wlan;
  udp.stop();
  bound = false;
  if (wlan) {
    check(radio.standby());
#ifdef TTGO_DUAL_TX
    part = bitmap::pieces;
    hasPeer = false;
    WiFi.mode(WIFI_AP);
    if (WiFi.softAP(ssid, password)) {
      message("WLAN H1 wartet auf H2");
    } else {
      message("AP Fehler: PRG");
    }
#else
    received = false;
    online = showingImage = false;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
    message("Suche WLAN Life-H1");
#endif
  } else {
    WiFi.mode(WIFI_OFF);
#ifdef TTGO_DUAL_RX
    receiver.mask = 0;
    received = false;
    check(radio.startReceive());
#endif
    message("LoRa Vollbild");
  }
}

void setup() {
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(0), buttonDown, FALLING);
  Serial.begin(115200);
  oled.init();
  oled.flipScreenVertically();
  SPI.begin(5, 19, 27, 18);
  check(radio.begin(869.525, 125.0, 7, 5, 0x12, 2, 8));
  check(radio.setCRC(true));
#ifdef TTGO_DUAL_TX
  frame = esp_random();
  startGame();
#else
  message("RX wartet auf Bild");
  radio.setPacketReceivedAction(onReceive);
  check(radio.startReceive());
#endif
}
static void handleWifi() {
#ifndef TTGO_DUAL_TX
  if (WiFi.status() != WL_CONNECTED) {
    if (online) {
      udp.stop(); bound = online = showingImage = false;
      message("Suche WLAN Life-H1");
    }
    return;
  }
  if (!online) {
    online = true;
    message("WLAN: warte auf Bild");
    lastHello = millis() - 1000;
  }
#endif
  if (!bound) {
    bound = udp.begin(port);
    if (!bound) return;
  }
  const int size = udp.parsePacket();
#ifdef TTGO_DUAL_TX
  if (size > 0) {
    char hello[4];
    const IPAddress sender = udp.remoteIP();
    const int count = udp.read(reinterpret_cast<uint8_t*>(hello), sizeof hello);
    udp.flush();
    if (size == 4 && count == 4 && !memcmp(hello, "H2V1", 4)) {
      peer = sender; hasPeer = true; lastHello = millis();
    }
  }
  if (hasPeer && millis() - lastHello > 3000) {
    hasPeer = false;
    message("WLAN H1 wartet auf H2");
  }
  if (!hasPeer || millis() - lastFrame < 100) return;
  lastFrame = millis();
  prepareImage();
  memcpy(wireFrame.pixels, image, sizeof image);
  if (udp.beginPacket(peer, port)) {
    udp.write(reinterpret_cast<const uint8_t*>(&wireFrame), sizeof wireFrame);
    udp.endPacket();
  }
  if (game.generation >= 499) startGame();
  else game.step();
#else
  if (millis() - lastHello >= 1000) {
    lastHello = millis();
    if (udp.beginPacket(WiFi.gatewayIP(), port)) {
      udp.write(reinterpret_cast<const uint8_t*>("H2V1"), 4);
      udp.endPacket();
    }
  }
  if (size > 0) {
    const bool fromH1 = udp.remoteIP() == WiFi.gatewayIP();
    const int count = udp.read(reinterpret_cast<uint8_t*>(&wireFrame), sizeof wireFrame);
    udp.flush();
    if (fromH1 && size == sizeof wireFrame && count == sizeof wireFrame &&
        !memcmp(wireFrame.magic, "GLW1", 4)) {
      displayImage(wireFrame.pixels);
      lastImage = millis(); showingImage = true;
    }
  }
  if (showingImage && millis() - lastImage > 3000) {
    showingImage = false; message("WLAN: warte auf Bild");
  }
#endif
}
static void handleLoRa() {
#ifdef TTGO_DUAL_TX
  if (part == bitmap::pieces) {
    if (sentLoRa && millis() - lastLoRa < 25000) return;
    sentLoRa = true; lastLoRa = millis();
    ++frame;
    prepareImage(); part = 0;
  }
  bitmap::encode(packet, image, frame, part);
  check(radio.transmit(packet, sizeof packet));
  delay(25);
  if (++part == bitmap::pieces) {
    if (game.generation >= 499) startGame();
    else game.step();
  }
#else
  if (!received) return;
  received = false;
  const size_t length = radio.getPacketLength();
  const int status = radio.readData(packet, sizeof packet);
  check(radio.startReceive());
  if (status == RADIOLIB_ERR_NONE && receiver.accept(packet, length))
    displayImage(receiver.pending);
#endif
}
void loop() {
  if (Serial.available() && Serial.read() == 'w') switchMode();
  if (buttonEdge) {
    buttonEdge = false;
    if (millis() - lastButton > 220) {
      lastButton = millis();
      switchMode();
    }
  }
  if (wlan) handleWifi();
  else handleLoRa();
  delay(1);
}
#endif
