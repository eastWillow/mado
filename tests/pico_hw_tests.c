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
