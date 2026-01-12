/*
 * Copyright (C) 2025 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <nba/device/audio_device.hpp>

namespace nba {

/**
 * WebAudio-based audio device for the WASM port.
 * Audio processing is done via AudioWorklet in JavaScript.
 * This class provides a callback interface that JavaScript can call
 * to get audio samples from the emulator.
 */
struct WASMAudioDevice : AudioDevice {
  WASMAudioDevice();
  ~WASMAudioDevice() override;

  auto GetSampleRate() -> int override;
  auto GetBlockSize() -> int override;
  bool Open(void* userdata, Callback callback) override;
  void SetPause(bool value) override;
  void Close() override;

  // Called from JavaScript to fill audio buffer
  void FillAudioBuffer(s16* buffer, int samples);

  // Check if audio is paused
  bool IsPaused() const { return paused; }

private:
  static constexpr int kSampleRate = 32768;
  static constexpr int kBlockSize = 1024;

  Callback callback = nullptr;
  void* userdata = nullptr;
  bool opened = false;
  bool paused = true;
};

} // namespace nba
