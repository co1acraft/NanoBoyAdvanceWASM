/*
 * Copyright (C) 2025 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include "wasm_audio_device.hpp"

namespace nba {

WASMAudioDevice::WASMAudioDevice() = default;

WASMAudioDevice::~WASMAudioDevice() {
  Close();
}

auto WASMAudioDevice::GetSampleRate() -> int {
  return kSampleRate;
}

auto WASMAudioDevice::GetBlockSize() -> int {
  return kBlockSize;
}

bool WASMAudioDevice::Open(void* userdata, Callback callback) {
  this->userdata = userdata;
  this->callback = callback;
  opened = true;
  paused = true;
  return true;
}

void WASMAudioDevice::SetPause(bool value) {
  paused = value;
}

void WASMAudioDevice::Close() {
  opened = false;
  callback = nullptr;
  userdata = nullptr;
}

void WASMAudioDevice::FillAudioBuffer(s16* buffer, int samples) {
  if(callback && opened && !paused) {
    callback(userdata, buffer, samples * 2 * sizeof(s16));
  } else {
    // Fill with silence
    for(int i = 0; i < samples * 2; i++) {
      buffer[i] = 0;
    }
  }
}

} // namespace nba
