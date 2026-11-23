#include <stdint.h>
#define I2C_SDA_GPIO 4
#define I2C_SCL_GPIO 5
#define NFC_IRQ_GPIO 6
#define BUTTON_WORN_GPIO 7
#define BUTTON_REPAIR_GPIO 8
#define EVENT_QUEUE_CAPACITY 64
void app_main(void) { /* Read NFC UID only after explicit tap; queue event; BLE sync to hub. */ }
