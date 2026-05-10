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
