/**
 * OV5640 camera driver + 5-band LED control.
 * Production: uses ESP-IDF esp_camera driver (DVP 8-bit parallel).
 * License: MIT
 */

#include "camera_driver.h"
#include <string.h>

static bool g_initialized = false;
static uint16_t g_width = 640;
static uint16_t g_height = 480;

void camera_init(void)
{
    /* Production: esp_camera_init() with DVP config:
     * .pin_d0-d7 = GPIO4-7,15-18
     * .pin_pclk = GPIO12, .pin_vsync = GPIO13, .pin_href = GPIO14
     * .pin_sccb_sda = GPIO9, .pin_sccb_scl = GPIO8
     * .pin_xclk = GPIO10, .pin_reset = GPIO11
     * .xclk_freq_hz = 20000000
     * .frame_format = PIXFORMAT_RGB565
     * .frame_size = FRAMESIZE_VGA (640×480)
     */
    g_initialized = true;
}

void camera_set_resolution(uint16_t width, uint16_t height)
{
    g_width = width;
    g_height = height;
}

void camera_set_led(uint8_t band, bool on)
{
    /* Production: GPIO output to MOSFET gate for each LED:
     * BAND_WHITE → GPIO21
     * BAND_470NM → GPIO38
     * BAND_660NM → GPIO37
     * BAND_850NM → GPIO39
     * BAND_940NM → GPIO40
     */
    (void)band;
    (void)on;
}

void camera_set_status_led(uint32_t rgb)
{
    /* Production: WS2812B on GPIO1 */
    (void)rgb;
}

int camera_capture(camera_frame_t *frame)
{
    if (!g_initialized || frame == NULL) return -1;

    /* Production: esp_camera_fb_get() — returns framebuffer from DMA.
     * Copy pointer + metadata into frame struct.
     * esp_camera_fb_return() after processing. */

    memset(frame, 0, sizeof(*frame));
    frame->width = g_width;
    frame->height = g_height;
    frame->timestamp = esp_timer_get_time() / 1000;

    return 0;  /* success */
}