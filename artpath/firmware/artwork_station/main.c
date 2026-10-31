/* Hardware integration skeleton: implement NFC, I2S, BLE with the selected ESP-IDF release. */
#include <stdint.h>
#define ART_I2C_SDA_GPIO 4
#define ART_I2C_SCL_GPIO 5
#define ART_BUTTON_GPIO 2
#define ART_I2S_BCLK_GPIO 6
#define ART_I2S_LRCLK_GPIO 7
#define ART_I2S_DOUT_GPIO 8
#define ART_MAX_CONTENT_BYTES 4096u
int main(void) { /* Board drivers intentionally not included or target-built. */ return (int)(ART_MAX_CONTENT_BYTES == 0u); }
