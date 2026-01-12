#!/bin/bash
# Build script for NanoBoyAdvance WASM

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build-wasm"
OUTPUT_DIR="$PROJECT_ROOT/dist-wasm"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}  NanoBoyAdvance WASM Build Script${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""

# Check for Emscripten
if [ -z "$EMSDK" ]; then
    echo -e "${YELLOW}Warning: EMSDK environment variable not set.${NC}"
    echo "Looking for Emscripten in common locations..."
    
    # Try common paths
    if [ -f "$HOME/emsdk/emsdk_env.sh" ]; then
        source "$HOME/emsdk/emsdk_env.sh"
    elif [ -f "/opt/emsdk/emsdk_env.sh" ]; then
        source "/opt/emsdk/emsdk_env.sh"
    else
        echo -e "${RED}Error: Emscripten SDK not found.${NC}"
        echo ""
        echo "Please install Emscripten:"
        echo "  git clone https://github.com/emscripten-core/emsdk.git"
        echo "  cd emsdk"
        echo "  ./emsdk install latest"
        echo "  ./emsdk activate latest"
        echo "  source ./emsdk_env.sh"
        exit 1
    fi
fi

# Verify emcc is available
if ! command -v emcc &> /dev/null; then
    echo -e "${RED}Error: emcc not found in PATH.${NC}"
    echo "Make sure you've activated Emscripten with: source emsdk_env.sh"
    exit 1
fi

echo -e "${GREEN}Using Emscripten:${NC} $(emcc --version | head -n1)"
echo ""

# Clean build option
if [ "$1" == "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    rm -rf "$OUTPUT_DIR"
    echo "Done."
    exit 0
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${GREEN}Configuring CMake...${NC}"
emcmake cmake "$PROJECT_ROOT" \
    -DPLATFORM_WASM=ON \
    -DPLATFORM_QT=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3" \
    -DCMAKE_C_FLAGS="-O3"

# Build
echo ""
echo -e "${GREEN}Building...${NC}"
emmake make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Create output directory and copy files
echo ""
echo -e "${GREEN}Copying output files...${NC}"
mkdir -p "$OUTPUT_DIR"

# Copy WASM output
cp "$BUILD_DIR/src/platform/wasm/nba.js" "$OUTPUT_DIR/"
cp "$BUILD_DIR/src/platform/wasm/nba.wasm" "$OUTPUT_DIR/"

# Copy web assets
cp "$SCRIPT_DIR/web/"*.html "$OUTPUT_DIR/"
cp "$SCRIPT_DIR/web/"*.css "$OUTPUT_DIR/"
cp "$SCRIPT_DIR/web/"*.js "$OUTPUT_DIR/"

echo ""
echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}  Build complete!${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""
echo "Output files are in: $OUTPUT_DIR"
echo ""
echo "To test locally:"
echo "  cd $OUTPUT_DIR"
echo "  python3 -m http.server 8080"
echo "  # Then open http://localhost:8080 in your browser"
echo ""
