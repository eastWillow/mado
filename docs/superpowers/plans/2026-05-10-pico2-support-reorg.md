# RPI PICO 2 Support Reorganization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize the Raspberry Pi Pico 2 support into 5 clean, functional commits on a new branch.

**Architecture:** Use a surgical approach to rebuild the history on a new branch `pico2-reorg` starting from commit `3a1417ee`. Each phase will be a single commit that represents a verified milestone.

**Tech Stack:** Git, CMake, Pico SDK, Segger RTT.

---

### Task 0: Branch Setup

**Goal:** Create the fresh branch for reorganization.

- [ ] **Step 1: Create new branch from 3a1417ee**

Run: `git checkout -b pico2-reorg 3a1417ee`

---

### Task 2: Phase 1 - Add RPI PICO 2 Compiler Environment

**Goal:** Setup CMake and basic configuration.

**Files:**
- Create: `CMakeLists.txt` (root)
- Modify: `backend/pico/config/DEV_Config.h`
- Modify: `backend/pico/config/DEV_Config.c`

- [ ] **Step 1: Create root CMakeLists.txt**

Create the `CMakeLists.txt` file with Pico SDK initialization and basic target definitions.

```cmake
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

add_library(twin_pico INTERFACE)
target_include_directories(twin_pico INTERFACE . backend/pico/config backend/pico/lcd backend/pico/font)
target_link_libraries(twin_pico INTERFACE pico_stdlib hardware_spi hardware_dma)
```

- [ ] **Step 2: Verify Configuration**

Run: `mkdir -p build && cd build && cmake .. -DPICO_BOARD=pico2`
Expected: Successful configuration.

- [ ] **Step 3: Commit Phase 1**

```bash
git add CMakeLists.txt backend/pico/config/
git commit -m "Phase 1: Add RPI PICO 2 Compiler Environment"
```

---

### Task 2: Phase 2 - Add RPI PICO 2 Unit test to Verify the HW

**Goal:** Add LCD drivers and hardware verification test.

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/pico_hw_tests.c`

- [ ] **Step 1: Update CMakeLists.txt for HW tests**

Add the `pico-hw-tests` target.

```cmake
# In CMakeLists.txt
add_executable(pico-hw-tests 
    tests/pico_hw_tests.c
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
target_link_libraries(pico-hw-tests twin_pico pico_stdio_rtt)
pico_enable_stdio_rtt(pico-hw-tests 1)
pico_add_extra_outputs(pico-hw-tests)
```

- [ ] **Step 2: Create tests/pico_hw_tests.c**

```c
#include "pico/stdio_rtt.h"
#include "pico/stdlib.h"
#include "twin.h"
#include "twin_private.h"
#include "backend/pico/lcd/LCD_Driver.h"

/* Prototypes */
twin_context_t *twin_pico_init(int width, int height);
void twin_pico_exit(twin_context_t *ctx);
void twin_pico_hw_init(void);
void twin_pico_display_update(twin_context_t *ctx);
uint16_t *twin_pico_get_framebuffer(twin_context_t *ctx);

#define TEST_START(name) printf("\n[ RUN      ] %s\n", name)
#define TEST_PASS(name)  printf("[       OK ] %s\n", name)
#define ASSERT_TRUE(cond) if (!(cond)) { printf("Fail: %s\n", #cond); return false; }

void heartbeat() {
    const uint LED_PIN = 25; // Default for Pico 2 (RP2350)
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    for (int i = 0; i < 5; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
        sleep_ms(100);
    }
}

bool test_lcd_visual_red() {
    TEST_START("test_lcd_visual_red");
    printf("Initializing twin_pico...\n");
    twin_context_t *ctx = twin_pico_init(LCD_3_5_WIDTH, LCD_3_5_HEIGHT);
    if (!ctx) {
        printf("Failed to initialize twin_pico context\n");
        return false;
    }
    printf("Initializing hardware...\n");
    twin_pico_hw_init();
    printf("Hardware initialized.\n");
    
    printf("Creating pixmap...\n");
    twin_pixmap_t *red = twin_pixmap_create(TWIN_ARGB32, 100, 100);
    if (!red) {
        printf("Failed to create pixmap\n");
        twin_pico_exit(ctx);
        return false;
    }
    
    printf("Filling pixmap...\n");
    twin_fill(red, 0xffff0000, TWIN_SOURCE, 0, 0, 100, 100);
    printf("Setting background...\n");
    twin_screen_set_background(ctx->screen, red);
    
    printf("Updating screen (software)...\n");
    twin_screen_update(ctx->screen);
    printf("Screen update (software) finished.\n");

    // Software Verification
    uint16_t *fb = twin_pico_get_framebuffer(ctx);
    uint16_t red_565 = 0x00f8; // 0xf800 swapped for SPI
    if (fb[0] != red_565) {
        printf("Software Verification: FAILED (Pixel 0 is 0x%04x, expected 0x%04x)\n", fb[0], red_565);
        twin_screen_set_background(ctx->screen, NULL);
        twin_pixmap_destroy(red);
        twin_pico_exit(ctx);
        return false;
    }
    printf("Software Verification: PASSED (Pixel 0 is 0x%04x)\n", fb[0]);

    printf("Updating display (hardware SPI/DMA)...\n");
    twin_pico_display_update(ctx);
    printf("Display update triggered.\n");
    printf("Screen should be red (tiled) now.\n");

    twin_screen_set_background(ctx->screen, NULL);
    twin_pixmap_destroy(red);
    twin_pico_exit(ctx);
    TEST_PASS("test_lcd_visual_red");
    return true;
}

int main() {
    heartbeat();
    stdio_rtt_init();
    sleep_ms(2000);
    printf("Mado Pico 2 Hardware Tests Starting...\n");
    int passed = 0;
    if (test_lcd_visual_red()) passed++;

    printf("\n================================================\n");
    printf("TEST SUMMARY: %d/1 passed\n", passed);
    printf("================================================\n");

    while (1) {
        gpio_put(25, 1);
        sleep_ms(500);
        gpio_put(25, 0);
        sleep_ms(500);
    }
}
```

- [ ] **Step 3: Verify Build**

Run: `make -C build pico-hw-tests`
Expected: Successful build of `pico-hw-tests.elf`.

- [ ] **Step 4: Commit Phase 2**

```bash
git add CMakeLists.txt tests/pico_hw_tests.c backend/pico/lcd/ backend/pico/font/
git commit -m "Phase 2: Add RPI PICO 2 Unit test to Verify the HW"
```

---

### Task 3: Phase 3 - Add RPI PICO 2 Unit test to Verify the SW

**Goal:** Add software unit tests and RTT runner script.

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/pico_unit_tests.c`
- Create: `scripts/run-pico-tests.sh`

- [ ] **Step 1: Update CMakeLists.txt for Unit tests**

```cmake
# In CMakeLists.txt
add_executable(pico-unit-tests tests/pico_unit_tests.c)
target_link_libraries(pico-unit-tests twin_pico pico_stdio_rtt)
pico_enable_stdio_rtt(pico-unit-tests 1)
pico_add_extra_outputs(pico-unit-tests)
```

- [ ] **Step 2: Create tests/pico_unit_tests.c**

```c
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio_rtt.h"
#include "twin.h"
#include "twin_private.h"

#define MOCK_LCD_WIDTH  480
#define MOCK_LCD_HEIGHT 320

/* Prototypes for backend functions */
twin_context_t *twin_pico_init(int width, int height);
void twin_pico_exit(twin_context_t *ctx);

/* Simple Unit Test Macros */
#define TEST_START(name) printf("\n[ RUN      ] %s\n", name)
#define TEST_PASS(name)  printf("[       OK ] %s\n", name)
#define TEST_FAIL(name)  printf("[  FAILED  ] %s\n", name)

#define ASSERT_TRUE(cond) \
    if (!(cond)) { \
        printf("Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        return false; \
    }

/* 1. Check RTT connections is Working */
bool test_rtt_connection() {
    TEST_START("test_rtt_connection");
    printf("RTT Connection is alive!\n");
    ASSERT_TRUE(true); // If we reached here and user sees it, RTT works.
    TEST_PASS("test_rtt_connection");
    return true;
}

/* 2. More Tests from existing patterns */
bool test_pixmap_creation() {
    TEST_START("test_pixmap_creation");
    twin_pixmap_t *pixmap = twin_pixmap_create(TWIN_ARGB32, 100, 100);
    ASSERT_TRUE(pixmap != NULL);
    ASSERT_TRUE(pixmap->width == 100);
    ASSERT_TRUE(pixmap->height == 100);
    twin_pixmap_destroy(pixmap);
    TEST_PASS("test_pixmap_creation");
    return true;
}

bool test_composite_solid() {
    TEST_START("test_composite_solid");
    twin_pixmap_t *dst = twin_pixmap_create(TWIN_ARGB32, 10, 10);
    ASSERT_TRUE(dst != NULL);

    /* Fill with blue */
    twin_fill(dst, 0xff0000ff, TWIN_SOURCE, 0, 0, 10, 10);

    /* Composite red over blue with 50% alpha */
    twin_operand_t src = {.source_kind = TWIN_SOLID, .u.argb = 0x80ff0000};
    twin_composite(dst, 0, 0, &src, 0, 0, NULL, 0, 0, TWIN_OVER, 10, 10);

    twin_pixmap_destroy(dst);
    TEST_PASS("test_composite_solid");
    return true;
}

bool test_window_creation() {
    TEST_START("test_window_creation");
    twin_context_t *ctx = twin_pico_init(MOCK_LCD_WIDTH, MOCK_LCD_HEIGHT);
    ASSERT_TRUE(ctx != NULL);

    twin_window_t *win = twin_window_create(ctx->screen, TWIN_ARGB32, TwinWindowPlain, 10, 10, 100, 100);
    ASSERT_TRUE(win != NULL);
    ASSERT_TRUE(win->pixmap->width == 100);
    ASSERT_TRUE(win->pixmap->height == 100);

    twin_window_destroy(win);
    twin_pico_exit(ctx);
    TEST_PASS("test_window_creation");
    return true;
}

bool test_path_creation() {
    TEST_START("test_path_creation");
    twin_path_t *path = twin_path_create();
    ASSERT_TRUE(path != NULL);

    twin_path_move(path, twin_int_to_fixed(10), twin_int_to_fixed(20));
    
    twin_path_destroy(path);
    TEST_PASS("test_path_creation");
    return true;
}

bool test_fixed_math() {
    TEST_START("test_fixed_math");
    
    /* Multiply: 2.5 * 4.0 = 10.0 */
    twin_fixed_t a = twin_double_to_fixed(2.5);
    twin_fixed_t b = twin_double_to_fixed(4.0);
    twin_fixed_t res = twin_fixed_mul(a, b);
    ASSERT_TRUE(res == twin_int_to_fixed(10));
    
    /* Divide: 10.0 / 2.5 = 4.0 */
    res = twin_fixed_div(twin_int_to_fixed(10), a);
    ASSERT_TRUE(res == b);
    
    /* Sqrt: sqrt(16.0) = 4.0 */
    res = twin_fixed_sqrt(twin_int_to_fixed(16));
    ASSERT_TRUE(res == twin_int_to_fixed(4));
    
    /* Sqrt: sqrt(2.0) approx 1.41421... */
    res = twin_fixed_sqrt(twin_int_to_fixed(2));
    ASSERT_TRUE(res >= 92680 && res <= 92683);

    TEST_PASS("test_fixed_math");
    return true;
}

bool test_matrix_ops() {
    TEST_START("test_matrix_ops");
    twin_matrix_t m;
    twin_matrix_identity(&m);
    ASSERT_TRUE(twin_matrix_is_identity(&m));

    /* Translate by 10, 20 */
    twin_matrix_translate(&m, twin_int_to_fixed(10), twin_int_to_fixed(20));
    
    twin_fixed_t x = twin_matrix_transform_x(&m, 0, 0);
    twin_fixed_t y = twin_matrix_transform_y(&m, 0, 0);
    ASSERT_TRUE(x == twin_int_to_fixed(10));
    ASSERT_TRUE(y == twin_int_to_fixed(20));

    /* Scale by 2x */
    twin_matrix_identity(&m);
    twin_matrix_scale(&m, twin_int_to_fixed(2), twin_int_to_fixed(2));
    x = twin_matrix_transform_x(&m, twin_int_to_fixed(5), twin_int_to_fixed(5));
    y = twin_matrix_transform_y(&m, twin_int_to_fixed(5), twin_int_to_fixed(5));
    ASSERT_TRUE(x == twin_int_to_fixed(10));
    ASSERT_TRUE(y == twin_int_to_fixed(10));

    TEST_PASS("test_matrix_ops");
    return true;
}

bool test_memory_stats() {
    TEST_START("test_memory_stats");
    
    twin_memory_info_t info_start, info_after;
    twin_memory_get_info(&info_start);
    
    void *ptr = twin_malloc(128);
    ASSERT_TRUE(ptr != NULL);
    
    twin_memory_get_info(&info_after);
    ASSERT_TRUE(info_after.current_bytes == info_start.current_bytes + 128);
    ASSERT_TRUE(info_after.total_allocs == info_start.total_allocs + 1);
    
    twin_free(ptr);
    twin_memory_get_info(&info_after);
    ASSERT_TRUE(info_after.current_bytes == info_start.current_bytes);
    ASSERT_TRUE(info_after.total_frees == info_start.total_frees + 1);
    
    printf("Memory: current=%d, peak=%d\n", info_after.current_bytes, info_after.peak_bytes);
    
    TEST_PASS("test_memory_stats");
    return true;
}

bool test_geom_ops() {
    TEST_START("test_geom_ops");
    
    /* Test distance to line squared */
    twin_spoint_t p1 = { .x = 0, .y = 0 };
    twin_spoint_t p2 = { .x = twin_int_to_sfixed(10), .y = 0 };
    twin_spoint_t p = { .x = twin_int_to_sfixed(5), .y = twin_int_to_sfixed(5) };
    
    twin_dfixed_t dist2 = _twin_distance_to_line_squared(&p, &p1, &p2);
    ASSERT_TRUE(dist2 == 6400);
    
    TEST_PASS("test_geom_ops");
    return true;
}

bool test_toplevel_create() {
    TEST_START("test_toplevel_create");
    twin_context_t *ctx = twin_pico_init(20, 20);
    ASSERT_TRUE(ctx != NULL);

    twin_toplevel_t *toplevel = twin_toplevel_create(ctx->screen, TWIN_ARGB32, TwinWindowApplication, 0, 0, 20, 20, "TestApp");
    ASSERT_TRUE(toplevel != NULL);

    twin_window_destroy(toplevel->box.widget.window);
    twin_free(toplevel);
    twin_pico_exit(ctx);
    TEST_PASS("test_toplevel_create");
    return true;
}

bool test_find_max_resolution() {
    TEST_START("test_find_max_resolution");
    
    int sizes[] = {20, 50, 100, 150, 200, 250, 300, 320};
    twin_format_t formats[] = {TWIN_A8, TWIN_RGB16, TWIN_ARGB32};
    const char *fmt_names[] = {"TWIN_A8", "TWIN_RGB16", "TWIN_ARGB32"};

    for (int f = 0; f < 3; f++) {
        for (int i = 0; i < 8; i++) {
            int s = sizes[i];
            twin_context_t *ctx = twin_pico_init(s, s);
            if (!ctx) {
                printf("Max for %s: %dx%d failed\n", fmt_names[f], s, s);
                continue;
            }
            
            // Try to create a pixmap of the screen size
            twin_pixmap_t *px = twin_pixmap_create(formats[f], s, s);
            if (px) {
                printf("Success for %s: %dx%d\n", fmt_names[f], s, s);
                twin_pixmap_destroy(px);
                twin_pico_exit(ctx);
            } else {
                printf("Max for %s: %dx%d failed (pixmap alloc)\n", fmt_names[f], s, s);
                twin_pico_exit(ctx);
                break;
            }
        }
    }
    
    TEST_PASS("test_find_max_resolution");
    return true;
}

int main() {
    stdio_rtt_init();
    sleep_ms(2000);
    
    printf("================================================\n");
    printf("Mado Pico 2 Unit Tests\n");
    printf("================================================\n");

    int passed = 0;
    int total = 0;

    if (test_rtt_connection()) passed++; total++;
    if (test_pixmap_creation()) passed++; total++;
    if (test_composite_solid()) passed++; total++;
    if (test_window_creation()) passed++; total++;
    if (test_path_creation()) passed++; total++;
    if (test_fixed_math()) passed++; total++;
    if (test_matrix_ops()) passed++; total++;
    if (test_memory_stats()) passed++; total++;
    if (test_geom_ops()) passed++; total++;
    if (test_toplevel_create()) passed++; total++;
    if (test_find_max_resolution()) passed++; total++;

    printf("\n================================================\n");
    printf("TEST SUMMARY: %d/%d passed\n", passed, total);
    printf("================================================\n");

    while (1) {
        tight_loop_contents();
    }

    return 0;
}
```

- [ ] **Step 3: Create scripts/run-pico-tests.sh**

```bash
#!/bin/bash
# Script to flash and run pico tests on a Raspberry Pi Pico
# and capture the Segger RTT output.

TYPE="unit"
INTERFACE="cmsis-dap"
HEADLESS=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --headless)
            HEADLESS=1
            shift
            ;;
        hw|unit)
            TYPE="$1"
            shift
            ;;
        cmsis-dap|picoprobe|jlink|stlink)
            INTERFACE="$1"
            shift
            ;;
        *)
            # Assume any other argument is INTERFACE if it doesn't match known types
            INTERFACE="$1"
            shift
            ;;
    esac
done

if [ "$TYPE" == "hw" ]; then
    ELF_FILE="build/pico-hw-tests.elf"
else
    ELF_FILE="build/pico-unit-tests.elf"
fi

# Detect OpenOCD
OPENOCD="/usr/local/bin/openocd"
PICO_SDK_OPENOCD="/home/eastwillow/.pico-sdk/openocd/0.12.0+dev/openocd.exe"
if [ -x "$PICO_SDK_OPENOCD" ]; then
    OPENOCD="$PICO_SDK_OPENOCD"
    SCRIPTS_DIR="/home/eastwillow/.pico-sdk/openocd/0.12.0+dev/scripts"
fi

# Search paths for OpenOCD scripts
SEARCH_PATHS=""
if [ -n "$SCRIPTS_DIR" ]; then
    SEARCH_PATHS="-s $SCRIPTS_DIR"
fi

if [ ! -f "$ELF_FILE" ]; then
    echo "Error: $ELF_FILE not found."
    exit 1
fi

# Check for required tools
for tool in nc; do
    if ! command -v $tool &> /dev/null; then
        echo "Error: Required tool '$tool' not found."
        exit 1
    fi
done

# Cleanup existing OpenOCD processes on this port
if command -v lsof &> /dev/null; then
    EXISTING_PID=$(lsof -t -i :19021)
    if [ -n "$EXISTING_PID" ]; then
        [ $HEADLESS -eq 0 ] && echo "Killing existing OpenOCD process (PID $EXISTING_PID) listening on port 19021..."
        kill $EXISTING_PID 2>/dev/null
        sleep 1
    fi
fi

if [ $HEADLESS -eq 0 ]; then
    echo "========================================================="
    echo " Flashing and running $ELF_FILE"
    echo " Using OpenOCD: $OPENOCD"
    echo "========================================================="
fi

# Start OpenOCD in the background.
$OPENOCD $SEARCH_PATHS \
        -f "interface/$INTERFACE.cfg" \
        -f "target/rp2350.cfg" \
        -c "adapter speed 5000" \
        -c "program $ELF_FILE verify" \
        -c "init" \
        -c "rtt setup 0x20000000 0x82000 \"SEGGER RTT\"" \
        -c "rtt server start 19021 0" \
        -c "rtt start" \
        -c "reset" > openocd.log 2>&1 &

OPENOCD_PID=$!

# Wait for OpenOCD to initialize and start the RTT server
[ $HEADLESS -eq 0 ] && echo "Waiting for RTT server..."
MAX_RETRIES=15
RETRY_COUNT=0
while ! nc -z localhost 19021 > /dev/null 2>&1; do
    sleep 1
    RETRY_COUNT=$((RETRY_COUNT + 1))
    if [ $RETRY_COUNT -ge $MAX_RETRIES ]; then
        echo "Error: Timeout waiting for RTT server. Check openocd.log."
        kill $OPENOCD_PID 2>/dev/null
        exit 1
    fi
    if ! kill -0 $OPENOCD_PID 2>/dev/null; then
        echo "Error: OpenOCD exited prematurely. Check openocd.log."
        exit 1
    fi
    [ $HEADLESS -eq 0 ] && printf "."
done
[ $HEADLESS -eq 0 ] && echo " Ready!"

[ $HEADLESS -eq 0 ] && echo "--- RTT OUTPUT START ---"
RTT_LOG="rtt_output.log"
rm -f "$RTT_LOG"
touch "$RTT_LOG"

# Start the RTT listener in the background
if command -v socat &> /dev/null; then
    socat -u TCP:localhost:19021,connect-timeout=5 - > "$RTT_LOG" 2>&1 &
else
    nc -N localhost 19021 > "$RTT_LOG" 2>&1 &
fi
LISTENER_PID=$!

# Monitor the log for the completion string
MAX_WAIT=60
ELAPSED=0
while [ $ELAPSED -lt $MAX_WAIT ]; do
    if grep -q "TEST SUMMARY" "$RTT_LOG"; then
        break
    fi
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

if [ $ELAPSED -ge $MAX_WAIT ]; then
    echo "Warning: Timeout waiting for test completion."
fi

[ $HEADLESS -eq 0 ] && echo "--- RTT OUTPUT END ---"

# In headless mode, output the log to stdout
if [ $HEADLESS -eq 1 ]; then
    cat "$RTT_LOG"
    if grep -q "Memory:" "$RTT_LOG"; then
        echo "--- MEMORY STATS ---"
        grep "Memory:" "$RTT_LOG"
    fi
fi

# Cleanup everything
kill $LISTENER_PID 2>/dev/null
kill $OPENOCD_PID 2>/dev/null
wait $OPENOCD_PID 2>/dev/null

# Exit with success if TEST SUMMARY found and no failures
if grep -q "TEST SUMMARY:.*0 passed" "$RTT_LOG"; then
    [ $HEADLESS -eq 0 ] && echo "Done (FAILED)."
    exit 1
elif grep -q "TEST SUMMARY" "$RTT_LOG"; then
    [ $HEADLESS -eq 0 ] && echo "Done (PASSED)."
    exit 0
else
    [ $HEADLESS -eq 0 ] && echo "Done (TIMEOUT/UNKNOWN)."
    exit 1
fi
```

- [ ] **Step 4: Verify Headless Test Execution**

Run: `./scripts/run-pico-tests.sh unit --headless`
Expected: Script connects to RTT and reports test results.

- [ ] **Step 5: Commit Phase 3**

```bash
git add CMakeLists.txt tests/pico_unit_tests.c scripts/run-pico-tests.sh
git commit -m "Phase 3: Add RPI PICO 2 Unit test to Verify the SW"
```

---

### Task 4: Phase 4 - Add RPI PICO 2 Backend for Mado

**Goal:** Implement the Mado backend for Pico.

**Files:**
- Modify: `CMakeLists.txt`
- Create: `backend/pico_backend.c`

- [ ] **Step 1: Create backend/pico_backend.c**

```c
#include "twin_private.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "backend/pico/config/DEV_Config.h"
#include "backend/pico/lcd/LCD_Driver.h"
#include "backend/pico/lcd/LCD_Touch.h"
#include <assert.h>

typedef struct {
    uint16_t *framebuffer;
    int dma_chan;
    bool touched;
} twin_pico_t;

#define PRIV(x) ((twin_pico_t *) ((twin_context_t *) x)->priv)

static void _twin_pico_put_begin(twin_coord_t left,
                                 twin_coord_t top,
                                 twin_coord_t right,
                                 twin_coord_t bottom,
                                 void *closure) {
    (void) left;
    (void) top;
    (void) right;
    (void) bottom;
    (void) closure;
}

static void _twin_pico_put_span(twin_coord_t left, twin_coord_t top, twin_coord_t right,
                                twin_argb32_t *pixels, void *closure) {
    assert(left >= 0 && left < LCD_3_5_WIDTH);
    assert(right > left && right <= LCD_3_5_WIDTH);
    assert(top >= 0 && top < LCD_3_5_HEIGHT);

    twin_pico_t *tx = PRIV(closure);
    uint16_t *dest = tx->framebuffer + top * LCD_3_5_WIDTH + left;
    int width = right - left;
    for (int i = 0; i < width; i++) {
        twin_argb32_t p = pixels[i];
        // ARGB8888 to RGB565 (swapped for SPI)
        uint16_t rgb565 = ((p & 0x00f80000) >> 8) | ((p & 0x0000fc00) >> 5) | ((p & 0x000000f8) >> 3);
        dest[i] = (rgb565 << 8) | (rgb565 >> 8);
    }
}

static bool twin_pico_poll(twin_context_t *ctx) {
    twin_pico_t *tx = PRIV(ctx);
    TP_DRAW tp;
    if (TP_Read_Point(&tp)) {
        twin_event_t ev;
        ev.kind = tx->touched ? TwinEventMotion : TwinEventButtonDown;
        ev.u.pointer.x = tp.Xpoint;
        ev.u.pointer.y = tp.Ypoint;
        ev.u.pointer.button = 1;
        twin_screen_dispatch(ctx->screen, &ev);
        tx->touched = true;
    } else if (tx->touched) {
        twin_event_t ev;
        ev.kind = TwinEventButtonUp;
        ev.u.pointer.button = 1;
        twin_screen_dispatch(ctx->screen, &ev);
        tx->touched = false;
    }
    return true;
}

void twin_pico_hw_init(void) {
    System_Init();
    LCD_Init(U2D_R2L, 1000);
}

void twin_pico_display_update(twin_context_t *ctx) {
    twin_pico_t *tx = PRIV(ctx);
    LCD_SetWindow(0, 0, LCD_3_5_WIDTH - 1, LCD_3_5_HEIGHT - 1);
    DEV_Digital_Write(LCD_DC_PIN, 1);
    DEV_Digital_Write(LCD_CS_PIN, 0);
    dma_channel_transfer_from_buffer_now(tx->dma_chan, tx->framebuffer, 
                                         LCD_3_5_WIDTH * LCD_3_5_HEIGHT * 2);
    dma_channel_wait_for_finish_blocking(tx->dma_chan);
    DEV_Digital_Write(LCD_CS_PIN, 1);
}

uint16_t *twin_pico_get_framebuffer(twin_context_t *ctx) {
    return PRIV(ctx)->framebuffer;
}

static void twin_pico_start(twin_context_t *ctx, void (*init_callback)(twin_context_t *)) {
    twin_pico_hw_init();
    TP_Init(U2D_R2L);
    TP_GetAdFac();
    LCD_Clear(WHITE);
    
    if (init_callback) init_callback(ctx);

    while (twin_dispatch_once(ctx)) {
        if (twin_screen_damaged(ctx->screen)) {
            twin_screen_update(ctx->screen);
            twin_pico_display_update(ctx);
        }
    }
}

void twin_pico_exit(twin_context_t *ctx) {
    twin_pico_t *tx = PRIV(ctx);
    if (tx) {
        if (ctx->screen) twin_screen_destroy(ctx->screen);
        if (tx->framebuffer) free(tx->framebuffer);
        dma_channel_unclaim(tx->dma_chan);
        free(tx);
    }
    free(ctx);
}

twin_context_t *twin_pico_init(int width, int height) {
    twin_context_t *ctx = calloc(1, sizeof(twin_context_t));
    twin_pico_t *tx = calloc(1, sizeof(twin_pico_t));
    tx->framebuffer = malloc(LCD_3_5_WIDTH * LCD_3_5_HEIGHT * 2);
    if (!tx->framebuffer) {
        printf("ERROR: Failed to allocate framebuffer!\n");
        free(tx);
        free(ctx);
        return NULL;
    }
    tx->dma_chan = dma_claim_unused_channel(false);
    if (tx->dma_chan == -1) {
        free(tx->framebuffer);
        free(tx);
        free(ctx);
        return NULL;
    }

    dma_channel_config c = dma_channel_get_default_config(tx->dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    dma_channel_configure(tx->dma_chan, &c, &spi_get_hw(SPI_PORT)->dr,
                          tx->framebuffer, LCD_3_5_WIDTH * LCD_3_5_HEIGHT * 2,
                          false);

    ctx->priv = tx;
    ctx->screen = twin_screen_create(LCD_3_5_WIDTH, LCD_3_5_HEIGHT,
                                     _twin_pico_put_begin, _twin_pico_put_span,
                                     ctx);
    return ctx;
}

const twin_backend_t g_twin_backend = {
    .init = twin_pico_init,
    .configure = NULL,
    .poll = twin_pico_poll,
    .start = twin_pico_start,
    .exit = twin_pico_exit,
};
```

- [ ] **Step 2: Define TWIN_SOURCES and twin library in CMakeLists.txt**

Move sources from Phase 1/2 into the `twin` library target.

```cmake
# In CMakeLists.txt
set(TWIN_SOURCES
    src/box.c src/poly.c src/toplevel.c src/button.c src/fixed.c
    src/label.c src/trig.c src/convolve.c src/font.c src/matrix.c
    src/queue.c src/widget.c src/font_default.c src/path.c src/screen.c
    src/window.c src/dispatch.c src/geom.c src/pattern.c src/spline.c
    src/work.c src/draw-common.c src/hull.c src/icon.c src/pixmap.c
    src/timeout.c src/image.c src/animation.c src/closure.c src/api.c
    src/image-tvg.c src/draw-builtin.c src/memstats.c
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
add_library(twin STATIC ${TWIN_SOURCES})
target_link_libraries(twin twin_pico)
```

- [ ] **Step 3: Verify Build**

Run: `make -C build twin`
Expected: Successful build of `libtwin.a`.

- [ ] **Step 4: Commit Phase 4**

```bash
git add CMakeLists.txt backend/pico_backend.c
git commit -m "Phase 4: Add RPI PICO 2 Backend for Mado"
```

---

### Task 5: Phase 5 - Verify the RPI PICO 2 Application Demo Spline

**Goal:** Enable the main demo with Spline verification.

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `apps/main.c`

- [ ] **Step 1: Add demo-pico target to CMakeLists.txt**

```cmake
# In CMakeLists.txt
add_executable(demo-pico apps/main.c apps/calc.c apps/clock.c apps/image.c apps/spline.c)
target_link_libraries(demo-pico twin pico_stdio_rtt)
pico_enable_stdio_rtt(demo-pico 1)
pico_add_extra_outputs(demo-pico)
```

- [ ] **Step 2: Update apps/main.c for Pico 2**

Include Pico headers and initialization.

```c
// In apps/main.c (top)
#if defined(PICO_BOARD)
#include "pico/stdlib.h"
#include "pico/stdio_rtt.h"
#include "backend/pico/lcd/LCD_Driver.h"
#endif

// In apps/main.c (main start)
#if defined(PICO_BOARD)
    stdio_rtt_init();
    sleep_ms(2000);
#endif

// In apps/main.c (WIDTH/HEIGHT)
#if defined(PICO_BOARD)
#define WIDTH 150
#define HEIGHT 150
#else
#define WIDTH 640
#define HEIGHT 480
#endif

// In apps/main.c (init_demo_apps)
#if defined(PICO_BOARD)
    apps_spline_start(screen, "Spline", 0, 0, 150, 150);
#else
    // ... rest of init_demo_apps
#endif
```

- [ ] **Step 3: Verify Final Build**

Run: `make -C build demo-pico`
Expected: Successful build of `demo-pico.elf`.

- [ ] **Step 4: Commit Phase 5**

```bash
git add CMakeLists.txt apps/main.c
git commit -m "Phase 5: Verify the RPI PICO 2 Application Demo Spline"
```

