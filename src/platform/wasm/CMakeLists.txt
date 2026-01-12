cmake_minimum_required(VERSION 3.11...3.28)

set(SOURCES
  src/wasm_audio_device.cpp
  src/wasm_video_device.cpp
  src/emulator.cpp
)

set(HEADERS
  src/wasm_audio_device.hpp
  src/wasm_video_device.hpp
  src/wasm_backup.hpp
)

add_executable(nba-wasm ${SOURCES} ${HEADERS})

target_link_libraries(nba-wasm PRIVATE nba)
target_include_directories(nba-wasm PRIVATE src)

# Emscripten-specific settings
if(EMSCRIPTEN)
  set_target_properties(nba-wasm PROPERTIES
    OUTPUT_NAME "nba"
    SUFFIX ".js"
  )

  # Emscripten compilation and linking flags
  target_compile_options(nba-wasm PRIVATE
    -O3
  )

  target_link_options(nba-wasm PRIVATE
    -O3
    -sWASM=1
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=67108864
    -sMAXIMUM_MEMORY=536870912
    -sEXPORTED_FUNCTIONS=['_malloc','_free']
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','setValue','getValue','UTF8ToString','HEAPU8','HEAP16','HEAPU32']
    -sMODULARIZE=1
    -sEXPORT_NAME='createNBA'
    -sENVIRONMENT='web'
    -sASSERTIONS=1
    -sNO_FILESYSTEM=1
    -sSTACK_SIZE=1048576
    --bind
  )
endif()
