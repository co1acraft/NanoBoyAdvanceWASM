/*
 * Copyright (C) 2025 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <nba/core.hpp>
#include <nba/config.hpp>
#include <nba/rom/rom.hpp>

#include "wasm_audio_device.hpp"
#include "wasm_video_device.hpp"
#include "wasm_backup.hpp"

using namespace nba;

/**
 * WASM Emulator wrapper class that provides a clean interface for JavaScript.
 * Handles ROM loading, emulation, input, and save states.
 */
class Emulator {
public:
  Emulator() {
    config = std::make_shared<Config>();
    audio_device = std::make_shared<WASMAudioDevice>();
    video_device = std::make_shared<WASMVideoDevice>();
    config->audio_dev = audio_device;
    config->video_dev = video_device;
    core = CreateCore(config);
  }

  /**
   * Load BIOS from a byte array
   */
  bool LoadBIOS(const emscripten::val& data) {
    auto length = data["length"].as<size_t>();
    if(length != 0x4000) {
      return false;
    }

    bios_data.resize(length);
    for(size_t i = 0; i < length; i++) {
      bios_data[i] = data[i].as<u8>();
    }
    core->Attach(bios_data);
    bios_loaded = true;
    return true;
  }

  /**
   * Load ROM from a byte array
   */
  bool LoadROM(const emscripten::val& data, int backup_type_int = 0) {
    auto length = data["length"].as<size_t>();
    if(length < 192 || length > 32 * 1024 * 1024) {
      return false;
    }

    std::vector<u8> rom_data;
    rom_data.resize(length);
    for(size_t i = 0; i < length; i++) {
      rom_data[i] = data[i].as<u8>();
    }

    // Determine backup type
    auto backup_type = static_cast<Config::BackupType>(backup_type_int);
    if(backup_type == Config::BackupType::Detect) {
      backup_type = DetectBackupType(rom_data);
    }

    // Store backup type and size for save/load operations
    current_backup_type = backup_type;

    // Create backup memory using WASM-specific implementations
    auto backup = CreateBackup(backup_type);
    // Store raw pointer before moving - safe because ROM owns the backup
    current_backup = dynamic_cast<WASMBackup*>(backup.get());

    // Create ROM with mask for proper mirroring
    u32 rom_mask = 0x01FF'FFFF;
    size_t power_of_two = 1;
    while(power_of_two < length) {
      power_of_two <<= 1;
    }
    rom_mask = static_cast<u32>(power_of_two - 1);

    core->Attach(ROM{
      std::move(rom_data),
      std::move(backup),
      nullptr, // GPIO (RTC, solar sensor not yet implemented in WASM)
      rom_mask
    });

    rom_loaded = true;
    return true;
  }

  /**
   * Reset the emulator
   */
  void Reset() {
    core->Reset();
  }

  /**
   * Run one frame of emulation
   */
  void RunFrame() {
    if(rom_loaded) {
      core->RunForOneFrame();
    }
  }

  /**
   * Set key state
   * @param key Key index (A=0, B=1, Select=2, Start=3, Right=4, Left=5, Up=6, Down=7, R=8, L=9)
   * @param pressed true if pressed, false if released
   */
  void SetKeyState(int key, bool pressed) {
    if(key >= 0 && key < 10) {
      core->SetKeyStatus(static_cast<Key>(key), pressed);
    }
  }

  /**
   * Check if a new video frame is available
   */
  bool HasNewFrame() const {
    return video_device->HasNewFrame();
  }

  /**
   * Get pointer to the frame buffer (for use with Emscripten HEAP)
   */
  uintptr_t GetFrameBufferPtr() {
    video_device->ClearNewFrame();
    return reinterpret_cast<uintptr_t>(video_device->GetFrameBuffer());
  }

  /**
   * Fill audio buffer with samples
   * @param samples Number of stereo samples to generate
   */
  void FillAudioBuffer(uintptr_t buffer_ptr, int samples) {
    auto buffer = reinterpret_cast<s16*>(buffer_ptr);
    audio_device->FillAudioBuffer(buffer, samples);
  }

  /**
   * Set audio pause state
   */
  void SetAudioPause(bool paused) {
    audio_device->SetPause(paused);
  }

  /**
   * Get audio sample rate
   */
  int GetAudioSampleRate() const {
    return audio_device->GetSampleRate();
  }

  /**
   * Skip BIOS intro
   */
  void SetSkipBIOS(bool skip) {
    config->skip_bios = skip;
  }

  /**
   * Set audio volume (0-100)
   */
  void SetVolume(int volume) {
    config->audio.volume = std::max(0, std::min(100, volume));
  }

  /**
   * Check if emulator is ready to run
   */
  bool IsReady() const {
    return bios_loaded && rom_loaded;
  }

  /**
   * Get save data as Uint8Array for the current game
   */
  emscripten::val GetSaveData() {
    auto* backup = GetCurrentBackup();
    if(!backup) {
      return emscripten::val::null();
    }

    auto size = backup->GetSize();
    if(size == 0) {
      return emscripten::val::null();
    }

    auto result = emscripten::val::global("Uint8Array").new_(size);
    auto* data = backup->GetData();
    for(size_t i = 0; i < size; i++) {
      result.set(i, data[i]);
    }
    return result;
  }

  /**
   * Load save data from Uint8Array
   */
  bool LoadSaveData(const emscripten::val& data) {
    auto* backup = GetCurrentBackup();
    if(!backup) {
      return false;
    }

    auto size = backup->GetSize();
    auto data_size = data["length"].as<size_t>();
    
    if(data_size != size) {
      return false;
    }

    std::vector<u8> buffer(size);
    for(size_t i = 0; i < size; i++) {
      buffer[i] = data[i].as<u8>();
    }
    backup->SetData(buffer.data(), size);
    return true;
  }

  /**
   * Get the size of the save data
   */
  int GetSaveDataSize() {
    auto* backup = GetCurrentBackup();
    if(!backup) {
      return 0;
    }
    return static_cast<int>(backup->GetSize());
  }

  /**
   * Create a save state
   */
  emscripten::val CreateSaveState() {
    SaveState state;
    core->CopyState(state);
    
    // Serialize save state to bytes
    auto size = sizeof(SaveState);
    auto result = emscripten::val::global("Uint8Array").new_(size);
    auto* bytes = reinterpret_cast<const u8*>(&state);
    for(size_t i = 0; i < size; i++) {
      result.set(i, bytes[i]);
    }
    return result;
  }

  /**
   * Load a save state
   */
  bool LoadSaveState(const emscripten::val& data) {
    auto data_size = data["length"].as<size_t>();
    if(data_size != sizeof(SaveState)) {
      return false;
    }
    
    SaveState state;
    auto* bytes = reinterpret_cast<u8*>(&state);
    for(size_t i = 0; i < data_size; i++) {
      bytes[i] = data[i].as<u8>();
    }
    
    // Validate magic number and version
    if(state.magic != SaveState::kMagicNumber) {
      return false;
    }
    if(state.version != SaveState::kCurrentVersion) {
      return false;
    }
    
    core->LoadState(state);
    return true;
  }

  // Screen dimensions
  static constexpr int GetScreenWidth() { return 240; }
  static constexpr int GetScreenHeight() { return 160; }

private:
  Config::BackupType DetectBackupType(const std::vector<u8>& rom_data) {
    // Search for backup strings in ROM
    const std::vector<std::pair<std::string, Config::BackupType>> signatures = {
      {"EEPROM_V",   Config::BackupType::EEPROM_DETECT},
      {"SRAM_V",     Config::BackupType::SRAM},
      {"SRAM_F_V",   Config::BackupType::SRAM},
      {"FLASH_V",    Config::BackupType::FLASH_64},
      {"FLASH512_V", Config::BackupType::FLASH_64},
      {"FLASH1M_V",  Config::BackupType::FLASH_128}
    };

    for(const auto& [signature, type] : signatures) {
      auto it = std::search(rom_data.begin(), rom_data.end(),
                           signature.begin(), signature.end());
      if(it != rom_data.end()) {
        return type;
      }
    }

    return Config::BackupType::SRAM; // Default
  }

  std::unique_ptr<Backup> CreateBackup(Config::BackupType type) {
    switch(type) {
      case Config::BackupType::None:
        return nullptr;
      case Config::BackupType::SRAM:
        return std::make_unique<WASM_SRAM>();
      case Config::BackupType::FLASH_64:
        return std::make_unique<WASM_FLASH>(WASM_FLASH::SIZE_64K);
      case Config::BackupType::FLASH_128:
        return std::make_unique<WASM_FLASH>(WASM_FLASH::SIZE_128K);
      case Config::BackupType::EEPROM_4:
        return std::make_unique<WASM_EEPROM>(WASM_EEPROM::SIZE_4K, core->GetScheduler());
      case Config::BackupType::EEPROM_64:
      case Config::BackupType::EEPROM_DETECT:
        return std::make_unique<WASM_EEPROM>(WASM_EEPROM::SIZE_64K, core->GetScheduler());
      default:
        return std::make_unique<WASM_SRAM>();
    }
  }

  WASMBackup* GetCurrentBackup() {
    // The backup pointer is stored when loading ROM
    return current_backup;
  }

  std::shared_ptr<Config> config;
  std::shared_ptr<WASMAudioDevice> audio_device;
  std::shared_ptr<WASMVideoDevice> video_device;
  std::unique_ptr<CoreBase> core;
  std::vector<u8> bios_data;
  bool bios_loaded = false;
  bool rom_loaded = false;
  Config::BackupType current_backup_type = Config::BackupType::Detect;
  WASMBackup* current_backup = nullptr; // Non-owning pointer to backup in ROM
};

// Emscripten bindings
EMSCRIPTEN_BINDINGS(nba_emulator) {
  emscripten::class_<Emulator>("Emulator")
    .constructor<>()
    .function("loadBIOS", &Emulator::LoadBIOS)
    .function("loadROM", &Emulator::LoadROM)
    .function("reset", &Emulator::Reset)
    .function("runFrame", &Emulator::RunFrame)
    .function("setKeyState", &Emulator::SetKeyState)
    .function("hasNewFrame", &Emulator::HasNewFrame)
    .function("getFrameBufferPtr", &Emulator::GetFrameBufferPtr)
    .function("fillAudioBuffer", &Emulator::FillAudioBuffer)
    .function("setAudioPause", &Emulator::SetAudioPause)
    .function("getAudioSampleRate", &Emulator::GetAudioSampleRate)
    .function("setSkipBIOS", &Emulator::SetSkipBIOS)
    .function("setVolume", &Emulator::SetVolume)
    .function("isReady", &Emulator::IsReady)
    .function("getSaveData", &Emulator::GetSaveData)
    .function("loadSaveData", &Emulator::LoadSaveData)
    .function("getSaveDataSize", &Emulator::GetSaveDataSize)
    .function("createSaveState", &Emulator::CreateSaveState)
    .function("loadSaveState", &Emulator::LoadSaveState)
    .class_function("getScreenWidth", &Emulator::GetScreenWidth)
    .class_function("getScreenHeight", &Emulator::GetScreenHeight);
}
