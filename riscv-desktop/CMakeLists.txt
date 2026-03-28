cmake_minimum_required(VERSION 3.10)
project(riscv_emu C)

set(CMAKE_C_STANDARD 11)

if(MSVC)
  add_compile_options(/O2 /W3)
else()
  add_compile_options(-O2 -Wall -Wextra)
endif()

include_directories(src src/peripheral)

set(SOURCES
  src/main_cli.c
  src/emulator.c
  src/cpu.c
  src/bus.c
  src/memory.c
  src/waveform.c
  src/peripheral/gpio.c
  src/peripheral/timer.c
  src/peripheral/uart.c
)

add_executable(riscv_emu_cli ${SOURCES})

if(UNIX)
  target_link_libraries(riscv_emu_cli m)
endif()

set_target_properties(riscv_emu_cli PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build
)
