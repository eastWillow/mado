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
