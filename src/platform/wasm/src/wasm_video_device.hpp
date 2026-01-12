/*
 * Copyright (C) 2025 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <nba/device/video_device.hpp>
#include <array>

namespace nba {

/**
 * Video device for WASM that stores frames in a buffer
 * that can be read by JavaScript for rendering to canvas.
 */
struct WASMVideoDevice : VideoDevice {
  static constexpr int kScreenWidth = 240;
  static constexpr int kScreenHeight = 160;
  static constexpr int kBufferSize = kScreenWidth * kScreenHeight;

  WASMVideoDevice();
  ~WASMVideoDevice() override = default;

  void Draw(u32* buffer) override;

  // Get pointer to the frame buffer for JavaScript
  auto GetFrameBuffer() -> u32* { return frame_buffer.data(); }

  // Check if a new frame is ready
  bool HasNewFrame() const { return new_frame_available; }

  // Clear new frame flag (call after reading)
  void ClearNewFrame() { new_frame_available = false; }

private:
  std::array<u32, kBufferSize> frame_buffer{};
  bool new_frame_available = false;
};

} // namespace nba
