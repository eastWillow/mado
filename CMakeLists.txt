# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)
project(mado C CXX ASM)
pico_sdk_init()

set(TWIN_SOURCES
    src/box.c src/poly.c src/toplevel.c src/button.c src/fixed.c
    src/label.c src/trig.c src/convolve.c src/font.c src/matrix.c
    src/queue.c src/widget.c src/font_default.c src/path.c src/screen.c
    src/window.c src/dispatch.c src/geom.c src/pattern.c src/spline.c
    src/work.c src/draw-common.c src/hull.c src/icon.c src/pixmap.c
    src/timeout.c src/image.c src/animation.c src/closure.c src/api.c
    src/image-tvg.c src/draw-builtin.c src/memstats.c
)

set(CONFIG_LOGGING 1)
set(CONFIG_WINDOW_MANAGER 0)
set(CONFIG_RENDERER_BUILTIN 1)
set(CONFIG_LOADER_TVG 1)
set(CONFIG_MEMORY_STATS 1)

if(CONFIG_LOGGING)
    list(APPEND TWIN_SOURCES src/log.c)
endif()

add_library(twin STATIC ${TWIN_SOURCES})

target_include_directories(twin PUBLIC include src . backend/pico/config backend/pico/lcd backend/pico/font)
target_compile_definitions(twin PUBLIC
    CONFIG_LOGGING=${CONFIG_LOGGING}
    CONFIG_WINDOW_MANAGER=${CONFIG_WINDOW_MANAGER}
    CONFIG_RENDERER_BUILTIN=${CONFIG_RENDERER_BUILTIN}
    CONFIG_LOADER_TVG=${CONFIG_LOADER_TVG}
    CONFIG_MEMORY_STATS=${CONFIG_MEMORY_STATS}
)
target_link_libraries(twin pico_stdlib hardware_spi hardware_dma)

add_library(twin_pico INTERFACE)
target_include_directories(twin_pico INTERFACE . backend/pico/config backend/pico/lcd backend/pico/font)
target_link_libraries(twin_pico INTERFACE pico_stdlib hardware_spi hardware_dma)


add_executable(pico-hw-tests 
    tests/pico_hw_tests.c
    backend/pico_backend.c
    backend/pico/config/DEV_Config.c
    backend/pico/lcd/LCD_Driver.c
    backend/pico/lcd/LCD_Touch.c
    backend/pico/lcd/LCD_GUI.c
    backend/pico/font/font8.c
    backend/pico/font/font12.c
    backend/pico/font/font16.c
    backend/pico/font/font20.c
    backend/pico/font/font24.c
)
target_link_libraries(pico-hw-tests twin pico_stdio_rtt)
pico_enable_stdio_rtt(pico-hw-tests 1)
pico_add_extra_outputs(pico-hw-tests)

add_executable(pico-unit-tests 
    tests/pico_unit_tests.c
    backend/pico_backend.c
    backend/pico/config/DEV_Config.c
    backend/pico/lcd/LCD_Driver.c
    backend/pico/lcd/LCD_Touch.c
    backend/pico/lcd/LCD_GUI.c
    backend/pico/font/font8.c
    backend/pico/font/font12.c
    backend/pico/font/font16.c
    backend/pico/font/font20.c
    backend/pico/font/font24.c
)
target_link_libraries(pico-unit-tests twin pico_stdio_rtt)
pico_enable_stdio_rtt(pico-unit-tests 1)
pico_add_extra_outputs(pico-unit-tests)
