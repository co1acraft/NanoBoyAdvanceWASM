/*
 * Copyright (C) 2025 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include "wasm_video_device.hpp"
#include <cstring>

namespace nba {

WASMVideoDevice::WASMVideoDevice() {
  frame_buffer.fill(0);
}

void WASMVideoDevice::Draw(u32* buffer) {
  // Copy the GBA framebuffer to our internal buffer
  // The PPU outputs ARGB format (0xAARRGGBB), canvas expects ABGR in little-endian
  // For ImageData in JavaScript: bytes are [R, G, B, A] which is 0xAABBGGRR in u32
  for(int i = 0; i < kBufferSize; i++) {
    u32 pixel = buffer[i];
    // PPU format: 0xFFRRGGBB (ARGB)
    // Canvas format: 0xAABBGGRR (ABGR - RGBA bytes in little-endian memory)
    u8 r = (pixel >> 16) & 0xFF;
    u8 g = (pixel >> 8) & 0xFF;
    u8 b = (pixel >> 0) & 0xFF;
    u8 a = 0xFF;
    frame_buffer[i] = (a << 24) | (b << 16) | (g << 8) | r;
  }
  new_frame_available = true;
}

} // namespace nba
