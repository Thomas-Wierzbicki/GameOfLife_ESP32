#pragma once
#include <stdint.h>
#include <string.h>

namespace bitmap {
constexpr int width = 128, height = 64;
constexpr int pieces = 8, dataBytes = 128, headerBytes = 9;
constexpr int packetBytes = headerBytes + dataBytes;
using Image = uint8_t[pieces * dataBytes];

inline void encode(uint8_t *packet, const Image &image, uint32_t frame, uint8_t part) {
  memcpy(packet, "GLB1", 4);
  for (int i = 0; i < 4; ++i) packet[4+i] = frame >> (24-8*i);
  packet[8] = part;
  memcpy(packet + headerBytes, image + part * dataBytes, dataBytes);
}

struct Receiver {
  Image pending{};
  uint32_t frame = 0;
  uint8_t mask = 0;

  bool accept(const uint8_t *packet, size_t length) {
    if (length != packetBytes || memcmp(packet, "GLB1", 4) || packet[8] >= pieces)
      return false;
    uint32_t id = 0;
    for (int i = 4; i < 8; ++i) id = (id << 8) | packet[i];
    if (frame != id) { frame = id; mask = 0; }
    uint8_t part = packet[8], flag = uint8_t(1U << part);
    if (mask & flag) return false;
    memcpy(pending + part * dataBytes, packet + headerBytes, dataBytes);
    mask |= flag;
    return mask == 0xff;
  }
};
}
