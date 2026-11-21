#pragma once
#include <stdint.h>
#include <string.h>

struct LifeGrid {
  uint8_t cells[64][128]{}, next[64][128]{};
  uint32_t seed = 0;
  uint16_t generation = 0;

  void reset(uint32_t value) {
    seed = value;
    generation = 0;
    uint32_t rng = seed;
    for (auto &row : cells) for (auto &cell : row) {
      rng ^= rng << 13;
      rng ^= rng >> 17;
      rng ^= rng << 5;
      cell = rng & 1;
    }
  }

  void step() {
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 128; ++x) {
      int count = 0;
      for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        int yy = y + dy, xx = x + dx;
        if ((dx || dy) && unsigned(yy) < 64 && unsigned(xx) < 128)
          count += cells[yy][xx];
      }
      next[y][x] = count == 3 || (cells[y][x] && count == 2);
    }
    memcpy(cells, next, sizeof cells);
    ++generation;
  }
};
