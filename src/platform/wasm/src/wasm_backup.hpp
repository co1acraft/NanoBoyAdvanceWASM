/*
 * Copyright (C) 2025 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <nba/rom/backup/backup.hpp>
#include <nba/scheduler.hpp>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace nba {

/**
 * Base class for in-memory backup storage used in WASM.
 * Provides a common interface for getting/setting raw save data.
 */
struct WASMBackup : Backup {
  virtual ~WASMBackup() = default;
  
  virtual auto GetSize() const -> size_t = 0;
  virtual auto GetData() -> u8* = 0;
  virtual auto GetData() const -> const u8* = 0;
  virtual void SetData(const u8* data, size_t size) = 0;
};

/**
 * In-memory SRAM implementation for WASM (32KB)
 */
struct WASM_SRAM : WASMBackup {
  static constexpr size_t kSize = 32768;

  WASM_SRAM() {
    memory.resize(kSize, 0xFF);
  }

  void Reset() override {
    std::fill(memory.begin(), memory.end(), 0xFF);
  }

  auto Read(u32 address) -> u8 override {
    return memory[address & 0x7FFF];
  }

  void Write(u32 address, u8 value) override {
    memory[address & 0x7FFF] = value;
  }

  void LoadState(SaveState const& state) override {
    std::memcpy(memory.data(), state.backup.data, kSize);
  }

  void CopyState(SaveState& state) override {
    std::memcpy(state.backup.data, memory.data(), kSize);
  }

  auto GetSize() const -> size_t override { return kSize; }
  auto GetData() -> u8* override { return memory.data(); }
  auto GetData() const -> const u8* override { return memory.data(); }
  
  void SetData(const u8* data, size_t size) override {
    size_t copy_size = std::min(size, kSize);
    std::memcpy(memory.data(), data, copy_size);
  }

private:
  std::vector<u8> memory;
};

/**
 * In-memory FLASH implementation for WASM (64KB or 128KB)
 */
struct WASM_FLASH : WASMBackup {
  enum Size {
    SIZE_64K = 0,
    SIZE_128K = 1
  };

  static constexpr size_t kSize64K = 65536;
  static constexpr size_t kSize128K = 131072;

  WASM_FLASH(Size size_hint) : size(size_hint) {
    memory.resize(size == SIZE_64K ? kSize64K : kSize128K, 0xFF);
  }

  void Reset() override {
    std::fill(memory.begin(), memory.end(), 0xFF);
    current_bank = 0;
    phase = 0;
    enable_chip_id = false;
    enable_erase = false;
    enable_write = false;
    enable_select = false;
  }

  auto Read(u32 address) -> u8 override {
    address &= 0xFFFF;
    
    if(enable_chip_id) {
      // Return chip ID
      if(address == 0) return size == SIZE_64K ? 0x32 : 0x62;
      if(address == 1) return size == SIZE_64K ? 0x1B : 0x13;
      return 0xFF;
    }
    
    return memory[Physical(address)];
  }

  void Write(u32 address, u8 value) override {
    address &= 0xFFFF;
    
    if(enable_write) {
      memory[Physical(address)] = value;
      enable_write = false;
      return;
    }
    
    if(enable_select && address == 0) {
      current_bank = value & 1;
      enable_select = false;
      return;
    }
    
    HandleCommand(address, value);
  }

  void LoadState(SaveState const& state) override {
    auto flash_size = size == SIZE_64K ? kSize64K : kSize128K;
    std::memcpy(memory.data(), state.backup.data, flash_size);
    current_bank = state.backup.flash.current_bank;
    phase = state.backup.flash.phase;
    enable_chip_id = state.backup.flash.enable_chip_id;
    enable_erase = state.backup.flash.enable_erase;
    enable_write = state.backup.flash.enable_write;
    enable_select = state.backup.flash.enable_select;
  }

  void CopyState(SaveState& state) override {
    auto flash_size = size == SIZE_64K ? kSize64K : kSize128K;
    std::memcpy(state.backup.data, memory.data(), flash_size);
    state.backup.flash.current_bank = current_bank;
    state.backup.flash.phase = phase;
    state.backup.flash.enable_chip_id = enable_chip_id;
    state.backup.flash.enable_erase = enable_erase;
    state.backup.flash.enable_write = enable_write;
    state.backup.flash.enable_select = enable_select;
  }

  auto GetSize() const -> size_t override { return memory.size(); }
  auto GetData() -> u8* override { return memory.data(); }
  auto GetData() const -> const u8* override { return memory.data(); }
  
  void SetData(const u8* data, size_t data_size) override {
    size_t copy_size = std::min(data_size, memory.size());
    std::memcpy(memory.data(), data, copy_size);
  }

private:
  void HandleCommand(u32 address, u8 value) {
    switch(phase) {
      case 0:
        if(address == 0x5555 && value == 0xAA) phase = 1;
        break;
      case 1:
        if(address == 0x2AAA && value == 0x55) phase = 2;
        else phase = 0;
        break;
      case 2:
        phase = 0;
        if(address == 0x5555) {
          switch(value) {
            case 0x90: enable_chip_id = true; break;
            case 0xF0: enable_chip_id = false; break;
            case 0x80: enable_erase = true; break;
            case 0x10:
              if(enable_erase) {
                std::fill(memory.begin(), memory.end(), 0xFF);
                enable_erase = false;
              }
              break;
            case 0xA0: enable_write = true; break;
            case 0xB0:
              if(size == SIZE_128K) enable_select = true;
              break;
          }
        } else if(enable_erase && value == 0x30) {
          // Sector erase (4KB sectors)
          auto sector_base = Physical(address & 0xF000);
          std::fill(memory.begin() + sector_base, memory.begin() + sector_base + 0x1000, 0xFF);
          enable_erase = false;
        }
        break;
    }
  }

  auto Physical(u32 address) -> size_t {
    if(size == SIZE_128K) {
      return static_cast<size_t>(current_bank) * 65536 + (address & 0xFFFF);
    }
    return address & 0xFFFF;
  }

  Size size;
  std::vector<u8> memory;
  int current_bank = 0;
  int phase = 0;
  bool enable_chip_id = false;
  bool enable_erase = false;
  bool enable_write = false;
  bool enable_select = false;
};

/**
 * In-memory EEPROM implementation for WASM (512B or 8KB)
 */
struct WASM_EEPROM : WASMBackup {
  enum Size {
    SIZE_4K = 0,   // 512 bytes, 6-bit addressing
    SIZE_64K = 1,  // 8KB, 14-bit addressing
    DETECT = 2
  };

  static constexpr size_t kSize4K = 512;
  static constexpr size_t kSize64K = 8192;

  WASM_EEPROM(Size size_hint, core::Scheduler& scheduler) 
    : size_hint(size_hint), scheduler(scheduler) {
    detect_size = (size_hint == DETECT);
    size = (size_hint == SIZE_4K) ? kSize4K : kSize64K;
    memory.resize(size, 0xFF);
  }

  void Reset() override {
    std::fill(memory.begin(), memory.end(), 0xFF);
    state = STATE_ACCEPT_COMMAND;
    address = 0;
    serial_buffer = 0;
    transmitted_bits = 0;
  }

  auto Read(u32) -> u8 override {
    if(state & STATE_BUSY) return 0;
    
    if(state & STATE_READING) {
      int bit_index = 63 - transmitted_bits;
      if(bit_index >= 0) {
        u8 bit = (serial_buffer >> bit_index) & 1;
        transmitted_bits++;
        if(transmitted_bits == 64) {
          state = STATE_ACCEPT_COMMAND;
        }
        return bit;
      }
    }
    
    return 1; // Ready
  }

  void Write(u32, u8 value) override {
    if(state & STATE_BUSY) return;
    
    u8 bit = value & 1;
    
    if(state & STATE_ACCEPT_COMMAND) {
      serial_buffer = (serial_buffer << 1) | bit;
      transmitted_bits++;
      
      if(transmitted_bits == 2) {
        u8 command = serial_buffer & 3;
        if(command == 3) { // Read
          state = STATE_READ_MODE | STATE_GET_ADDRESS;
        } else if(command == 2) { // Write
          state = STATE_WRITE_MODE | STATE_GET_ADDRESS;
        }
        ResetSerialBuffer();
      }
    } else if(state & STATE_GET_ADDRESS) {
      serial_buffer = (serial_buffer << 1) | bit;
      transmitted_bits++;
      
      int addr_bits = (size == kSize4K) ? 6 : 14;
      if(transmitted_bits == addr_bits) {
        address = static_cast<int>(serial_buffer) * 8;
        ResetSerialBuffer();
        
        if(state & STATE_READ_MODE) {
          state = STATE_DUMMY_NIBBLE;
        } else {
          state = STATE_WRITING;
        }
      }
    } else if(state & STATE_DUMMY_NIBBLE) {
      transmitted_bits++;
      if(transmitted_bits == 4) {
        // Load 64 bits from EEPROM
        serial_buffer = 0;
        for(int i = 0; i < 8; i++) {
          serial_buffer = (serial_buffer << 8) | memory[address + i];
        }
        transmitted_bits = 0;
        state = STATE_READING;
      }
    } else if(state & STATE_WRITING) {
      serial_buffer = (serial_buffer << 1) | bit;
      transmitted_bits++;
      
      if(transmitted_bits == 64) {
        // Write 64 bits to EEPROM
        for(int i = 7; i >= 0; i--) {
          memory[address + i] = serial_buffer & 0xFF;
          serial_buffer >>= 8;
        }
        state = STATE_EAT_DUMMY;
        transmitted_bits = 0;
      }
    } else if(state & STATE_EAT_DUMMY) {
      // Eat the stop bit
      state = STATE_ACCEPT_COMMAND;
      transmitted_bits = 0;
      serial_buffer = 0;
    }
  }

  void LoadState(SaveState const& state) override {
    std::memcpy(memory.data(), state.backup.data, memory.size());
    this->state = state.backup.eeprom.state;
    address = state.backup.eeprom.address;
    serial_buffer = state.backup.eeprom.serial_buffer;
    transmitted_bits = state.backup.eeprom.transmitted_bits;
  }

  void CopyState(SaveState& state) override {
    std::memcpy(state.backup.data, memory.data(), memory.size());
    state.backup.eeprom.state = this->state;
    state.backup.eeprom.address = address;
    state.backup.eeprom.serial_buffer = serial_buffer;
    state.backup.eeprom.transmitted_bits = transmitted_bits;
  }

  void SetSizeHint(Size new_size) {
    if(detect_size && new_size != DETECT) {
      detect_size = false;
      size = (new_size == SIZE_4K) ? kSize4K : kSize64K;
      memory.resize(size, 0xFF);
    }
  }

  auto GetSize() const -> size_t override { return size; }
  auto GetData() -> u8* override { return memory.data(); }
  auto GetData() const -> const u8* override { return memory.data(); }
  
  void SetData(const u8* data, size_t data_size) override {
    size_t copy_size = std::min(data_size, memory.size());
    std::memcpy(memory.data(), data, copy_size);
  }

private:
  enum State {
    STATE_ACCEPT_COMMAND = 1 << 0,
    STATE_READ_MODE      = 1 << 1,
    STATE_WRITE_MODE     = 1 << 2,
    STATE_GET_ADDRESS    = 1 << 3,
    STATE_READING        = 1 << 4,
    STATE_DUMMY_NIBBLE   = 1 << 5,
    STATE_WRITING        = 1 << 6,
    STATE_EAT_DUMMY      = 1 << 7,
    STATE_BUSY           = 1 << 8
  };

  void ResetSerialBuffer() {
    serial_buffer = 0;
    transmitted_bits = 0;
  }

  Size size_hint;
  size_t size;
  std::vector<u8> memory;
  core::Scheduler& scheduler;
  bool detect_size;
  int state = STATE_ACCEPT_COMMAND;
  int address = 0;
  u64 serial_buffer = 0;
  int transmitted_bits = 0;
};

} // namespace nba
