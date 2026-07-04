#ifndef GLUCOSYNC_CAMERA_DRIVER_H
#define GLUCOSYNC_CAMERA_DRIVER_H

#include <stdint.h>

/* Spectral band identifiers */
#define BAND_WHITE  0
#define BAND_470NM  1
#define BAND_660NM  2
#define BAND_850NM  3
#define BAND_940NM  4

typedef struct {
    uint8_t *data;       /* pointer to frame buffer in PSRAM */
    uint16_t width;
    uint16_t height;
    uint8_t  band;       /* which spectral band */
    uint32_t timestamp;
} camera_frame_t;

void camera_init(void);
void camera_set_resolution(uint16_t width, uint16_t height);
void camera_set_led(uint8_t band, bool on);
void camera_set_status_led(uint32_t rgb);
int  camera_capture(camera_frame_t *frame);

#endif