# NanoBoyAdvance WebAssembly Port

This is the WebAssembly port of NanoBoyAdvance, allowing you to run the GBA emulator directly in your web browser.

## Features

- Full GBA emulation in the browser
- Audio support via Web Audio API
- Keyboard input support
- Save/load game saves to browser storage
- Import/export save files
- Adjustable display scaling
- Skip BIOS option

## Building

### Prerequisites

1. **Emscripten SDK**: Install from https://emscripten.org/docs/getting_started/downloads.html

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### Build Steps

1. Navigate to the WASM platform directory:
```bash
cd src/platform/wasm
```

2. Run the build script:
```bash
chmod +x build.sh
./build.sh
```

3. The output will be in `dist-wasm/` at the project root.

### Manual Build

```bash
mkdir build-wasm
cd build-wasm
emcmake cmake .. -DPLATFORM_WASM=ON -DPLATFORM_QT=OFF -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc)
```

## Running Locally

After building, start a local web server:

```bash
cd dist-wasm
python3 -m http.server 8080
```

Then open http://localhost:8080 in your browser.

## Usage

1. **Load BIOS**: Click "Load BIOS" and select your GBA BIOS file (gba_bios.bin, 16KB)
2. **Load ROM**: Click "Load ROM" and select a GBA ROM file (.gba)
3. **Start**: Click "Start" to begin emulation

### Controls

| Key | GBA Button |
|-----|------------|
| X | A |
| Z | B |
| Enter | Start |
| Shift | Select |
| Arrow Keys | D-Pad |
| A | L |
| S | R |

### Saving

- **Save to Browser**: Saves game data to IndexedDB
- **Load Save**: Loads previously saved data
- **Export Save**: Downloads save as .sav file
- **Import Save**: Loads a .sav file

## Browser Compatibility

Tested on:
- Chrome/Chromium 90+
- Firefox 90+
- Safari 14+
- Edge 90+

## Technical Notes

- The WASM module is compiled with `-O3` optimization
- Memory starts at 64MB and can grow up to 512MB
- Audio runs at 32768Hz (GBA native rate)
- Video renders at native 240x160, scaled via CSS

## License

Licensed under GPLv3. See LICENSE in the project root.
