/*
 * scanner_main.c — WashWise Stain Scanner Node (ESP32-S3)
 *
 * Handheld multispectral garment scanner.
 *
 * Responsibilities:
 * - Capture multispectral image (white + UV-A 365nm + IR 940nm)
 * - Run on-device CNN for fabric type classification (TFLite Micro)
 * - Stain detection from UV fluorescence (protein/biological) + IR (oil-based)
 * - Care label OCR (lightweight)
 * - Display results on 1.3" ST7789
 * - Send scan results to hub → cloud ML for detailed stain ID
 * - Deep sleep between scans (~5µA, ~1 week battery)
 *
 * Hardware:
 * - OV2640 camera (2MP, DVP parallel)
 * - White LED + UV-A 365nm LED + IR 940nm LED (illumination)
 * - 1.3" ST7789 (240×240) display (SPI)
 * - 3× capacitive touch buttons
 * - 1000mAh LiPo + MCP73831 charger
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi.h"
#include "driver/adc.h"

#include "mesh_protocol.h"

static const char *TAG = "SCANNER";

/* ---- Pin Definitions (ESP32-S3) ---- */
/* Camera (OV2640 via DVP — uses dedicated camera peripheral) */
/* Pins 5-21 reserved for camera parallel interface in production */

/* SX1261 radio (SPI2) */
#define PIN_SX1261_SCK    12
#define PIN_SX1261_MOSI   13
#define PIN_SX1261_MISO   14
#define PIN_SX1261_CS     15
#define PIN_SX1261_BUSY   16
#define PIN_SX1261_IRQ    17
#define PIN_SX1261_NRST   18

/* Display ST7789 (SPI3) */
#define PIN_DISP_SCK      35
#define PIN_DISP_MOSI     36
#define PIN_DISP_CS       37
#define PIN_DISP_DC       38
#define PIN_DISP_RST      39
#define PIN_DISP_BL       40

/* Illumination LEDs */
#define PIN_LED_WHITE     41   /* white LED for color imaging */
#define PIN_LED_UV       42   /* UV-A 365nm LED for fluorescence */
#define PIN_LED_IR       2    /* IR 940nm LED for oil stain detection */

/* Capacitive touch buttons */
#define PIN_BTN_SCAN     3
#define PIN_BTN_NAV      4
#define PIN_BTN_CONFIRM  5

/* Battery */
#define ADC_BATTERY      ADC1_CHANNEL_4

/* ---- Fabric Types ---- */
enum {
    FABRIC_UNKNOWN = 0,
    FABRIC_COTTON,
    FABRIC_POLYESTER,
    FABRIC_WOOL,
    FABRIC_SILK,
    FABRIC_DENIM,
    FABRIC_NYLON,
    FABRIC_LINEN,
    FABRIC_BLEND
};

/* ---- Stain Types ---- */
enum {
    STAIN_CLEAN = 0,
    STAIN_COFFEE,
    STAIN_WINE,
    STAIN_BLOOD,
    STAIN_GREASE,
    STAIN_GRASS,
    STAIN_INK,
    STAIN_FOOD,
    STAIN_SWEAT,
    STAIN_RUST,
    STAIN_UNKNOWN
};

/* ---- Pre-treatment recommendations ---- */
static const struct {
    uint8_t stain;
    const char *name;
    const char *pre_treatment;
    uint8_t wash_temp_c;     /* recommended wash temp */
    uint8_t recommended_cycle;
    uint8_t detergent_ml;
} stain_db[] = {
    { STAIN_CLEAN,   "Clean",          "None needed",                    30, 0, 25 },
    { STAIN_COFFEE,  "Coffee/Tea",     "Rinse cold, enzyme detergent",    40, 0, 35 },
    { STAIN_WINE,   "Red Wine",       "Salt+club soda, white vinegar",  30, 1, 40 },
    { STAIN_BLOOD,  "Blood",          "COLD water only, hydrogen peroxide", 20, 1, 35 },
    { STAIN_GREASE, "Grease/Oil",     "Dish soap, rub gently",           40, 2, 45 },
    { STAIN_GRASS,  "Grass",          "Rubbing alcohol, enzyme detergent", 40, 0, 40 },
    { STAIN_INK,    "Ink",            "Hairspray or rubbing alcohol",    30, 1, 35 },
    { STAIN_FOOD,   "Food",           "Scrape, rinse cold, enzyme det.", 40, 0, 35 },
    { STAIN_SWEAT,  "Sweat",          "White vinegar soak, enzyme det.", 40, 0, 35 },
    { STAIN_RUST,   "Rust",           "Lemon + salt, commercial rust remover", 30, 1, 40 },
    { STAIN_UNKNOWN,"Unknown stain",  "Treat as grease: dish soap",      30, 0, 35 },
};

/* Fabric care recommendations */
static const struct {
    uint8_t fabric;
    const char *name;
    uint8_t max_temp_c;
    uint8_t cycle;
    uint8_t detergent_ml;
    bool bleach_safe;
    bool tumble_dry;
} fabric_db[] = {
    { FABRIC_UNKNOWN,  "Unknown",   40, 0, 30, true,  true  },
    { FABRIC_COTTON,   "Cotton",    60, 0, 35, true,  true  },
    { FABRIC_POLYESTER,"Polyester", 40, 1, 25, false, true  },
    { FABRIC_WOOL,     "Wool",      30, 3, 20, false, false },
    { FABRIC_SILK,     "Silk",      30, 3, 15, false, false },
    { FABRIC_DENIM,    "Denim",     40, 0, 35, false, true  },
    { FABRIC_NYLON,    "Nylon",     30, 1, 25, false, true  },
    { FABRIC_LINEN,    "Linen",     40, 1, 30, true,  true  },
    { FABRIC_BLEND,    "Blend",     40, 0, 30, false, true  },
};

/* ---- SX1261 Radio (stub) ---- */
static void sx1261_init(void) {
    ESP_LOGI(TAG, "SX1261 initialized (868MHz LoRa)");
}
static void sx1261_send(const uint8_t *data, uint16_t len) {}

/* ---- Display (stub) ---- */
static void display_init(void) {
    ESP_LOGI(TAG, "ST7789 display initialized");
}

static void display_scan_result(uint8_t fabric, uint8_t stain, uint8_t conf,
                                uint8_t temp, uint8_t cycle, uint8_t det_ml)
{
    const char *fab_name = "Unknown";
    const char *stain_name = "None";
    const char *cycle_name = "Normal";

    for (int i = 0; i < 9; i++) {
        if (fabric_db[i].fabric == fabric) { fab_name = fabric_db[i].name; break; }
    }
    for (int i = 0; i < 11; i++) {
        if (stain_db[i].stain == stain) { stain_name = stain_db[i].name; break; }
    }
    switch (cycle) {
        case 0: cycle_name = "Normal"; break;
        case 1: cycle_name = "Delicate"; break;
        case 2: cycle_name = "Heavy"; break;
        case 3: cycle_name = "Hand Wash"; break;
        case 4: cycle_name = "Quick"; break;
    }

    ESP_LOGI(TAG, "=== SCAN RESULT ===");
    ESP_LOGI(TAG, "Fabric: %s (%d%% conf)", fab_name, (conf * 100) / 255);
    ESP_LOGI(TAG, "Stain:  %s", stain_name);
    ESP_LOGI(TAG, "Recommend: %s cycle, %dC, %d mL detergent", cycle_name, temp, det_ml);

    /* In production: render to ST7789 display with icons + color */
}

/* ---- Camera capture (stub) ---- */

typedef struct {
    uint8_t white[240 * 240];  /* downsampled white-light image */
    uint8_t uv[240 * 240];     /* UV fluorescence image */
    uint8_t ir[240 * 240];     /* IR reflectance image */
} multispectral_image_t;

static void camera_init(void)
{
    /* In production: esp_camera_init with OV2640 config
     * - PIXFORMAT_RGB565
     * - FRAMESIZE_QVGA (240x320), crop to 240x240
     * - FB_LOC=internal
     */
    ESP_LOGI(TAG, "OV2640 camera initialized");
}

static void capture_multispectral(multispectral_image_t *img)
{
    /* In production:
     * 1. Turn on WHITE LED, capture frame → grayscale
     * 2. Turn on UV LED (365nm), capture frame → fluorescence
     * 3. Turn on IR LED (940nm), capture frame → reflectance
     * 4. Turn off all LEDs
     */
    ESP_LOGI(TAG, "Capturing multispectral images (white/UV/IR)");

    gpio_set_level(PIN_LED_WHITE, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    /* esp_camera_fb_get() → convert to grayscale → img->white */
    gpio_set_level(PIN_LED_WHITE, 0);

    gpio_set_level(PIN_LED_UV, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    /* esp_camera_fb_get() → img->uv */
    gpio_set_level(PIN_LED_UV, 0);

    gpio_set_level(PIN_LED_IR, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    /* esp_camera_fb_get() → img->ir */
    gpio_set_level(PIN_LED_IR, 0);

    /* Stub: fill with placeholder data */
    memset(img->white, 128, sizeof(img->white));
    memset(img->uv, 20, sizeof(img->uv));    /* low fluorescence = clean */
    memset(img->ir, 100, sizeof(img->ir));
}

/* ---- Fabric Classification (TFLite Micro stub) ---- */

static uint8_t classify_fabric(const multispectral_image_t *img, uint8_t *confidence)
{
    /* In production: TFLite Micro CNN model (~85KB)
     * Input: 64x64x3 (white/UV/IR channels, downsampled)
     * Model: 4 conv layers + 2 dense
     * Output: 9 fabric classes + confidence
     *
     * Key discriminators:
     * - Cotton: moderate UV fluorescence, distinct weave texture
     * - Polyester: low UV fluorescence, smooth surface
     * - Wool: strong UV fluorescence (protein), fuzzy texture
     * - Silk: strong UV fluorescence, smooth shiny
     * - Denim: low fluorescence, heavy twill texture
     * - Nylon: moderate fluorescence, smooth
     * - Linen: moderate fluorescence, visible weave
     *
     * Stub: return cotton with high confidence */
    *confidence = 230;  /* ~90% */
    return FABRIC_COTTON;
}

/* ---- Stain Detection (stub) ---- */

static uint8_t detect_stain(const multispectral_image_t *img, uint8_t *confidence)
{
    /* In production: TFLite Micro MobileNetV3-Small (~180KB)
     * Input: 3 spectral channels at detected stain region
     * Output: 11 stain classes
     *
     * Key discriminators:
     * - Protein stains (blood, sweat, food): UV fluorescence (bright spots)
     * - Oil/grease: IR reflectance (dark in IR, bright in white)
     * - Tannin stains (coffee, wine): dark in white, UV dark
     * - Ink: dark in all spectra
     * - Grass: green in white, UV fluorescence
     *
     * Stub: check UV brightness for fluorescence */
    uint32_t uv_sum = 0;
    for (int i = 0; i < 240 * 240; i++) {
        uv_sum += img->uv[i];
    }
    uint8_t avg_uv = uv_sum / (240 * 240);

    if (avg_uv < 30) {
        *confidence = 240;
        return STAIN_CLEAN;  /* low fluorescence = clean */
    }
    /* If fluorescence detected, classify as biological stain */
    *confidence = 200;
    return STAIN_UNKNOWN;
}

/* ---- Care Label OCR (stub) ---- */

static void extract_care_labels(uint8_t *care_label_buf)
{
    /* In production: lightweight OCR (Tesseract or custom)
     * detects standard ISO care symbols:
     * 0x01 = machine wash
     * 0x02 = hand wash
     * 0x03 = do not wash
     * 0x04 = tumble dry
     * 0x05 = do not tumble dry
     * 0x06 = iron low
     * 0x07 = iron medium
     * 0x08 = iron high
     * 0x09 = do not iron
     * 0x0A = bleach
     * 0x0B = do not bleach
     */
    memset(care_label_buf, 0, 8);
    care_label_buf[0] = 0x01;  /* machine wash */
    care_label_buf[1] = 0x04;  /* tumble dry */
    care_label_buf[2] = 0x0A;  /* bleach ok */
}

/* ---- Send Scan Result to Hub ---- */

static void send_scan_result(const scan_result_payload_t *result)
{
    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(
        NODE_ID_SCANNER, NODE_ID_HUB, PKT_SCAN_RESULT,
        (const uint8_t *)result, sizeof(*result), &pkt);
    sx1261_send((uint8_t *)&pkt, len);
    ESP_LOGI(TAG, "Scan result sent to hub");
}

/* ---- Battery ---- */

static uint8_t read_battery_pct(void)
{
    /* ADC: 3.0V=0%, 4.2V=100% */
    return 78;
}

/* ---- Scan Task ---- */

void scan_task(void *arg)
{
    ESP_LOGI(TAG, "WashWise Scanner Node starting");

    while (1) {
        /* Wait for scan button press (GPIO interrupt wakes from light sleep) */
        ESP_LOGI(TAG, "Ready. Press SCAN button to scan garment.");

        /* In production: configure GPIO wake + enter light sleep */
        /* For stub: wait on button check */
        bool scan_pressed = false;

        /* Poll button (stub) */
        if (gpio_get_level(PIN_BTN_SCAN) == 0) {
            scan_pressed = true;
        }

        if (!scan_pressed) {
            /* Deep sleep to save battery (~5µA)
             * Wake on GPIO (scan button) or timer (heartbeat every 10 min) */
            esp_sleep_enable_gpio_wakeup();
            gpio_wakeup_enable(PIN_BTN_SCAN, GPIO_INTR_LOW_LEVEL);
            esp_deep_sleep_start();
            /* Returns here after wake */
            continue;
        }

        ESP_LOGI(TAG, "=== SCAN STARTED ===");

        /* Capture multispectral images */
        multispectral_image_t img;
        capture_multispectral(&img);

        /* Classify fabric */
        uint8_t fab_conf;
        uint8_t fabric = classify_fabric(&img, &fab_conf);

        /* Detect stain */
        uint8_t stain_conf;
        uint8_t stain = detect_stain(&img, &stain_conf);

        /* Extract care labels */
        uint8_t care_labels[8];
        extract_care_labels(care_labels);

        /* Get recommendations from lookup tables */
        uint8_t wash_temp = 30;
        uint8_t cycle = 0;
        uint8_t det_ml = 30;

        for (int i = 0; i < 9; i++) {
            if (fabric_db[i].fabric == fabric) {
                wash_temp = fabric_db[i].max_temp_c;
                cycle = fabric_db[i].cycle;
                det_ml = fabric_db[i].detergent_ml;
                break;
            }
        }

        /* Adjust for stain */
        for (int i = 0; i < 11; i++) {
            if (stain_db[i].stain == stain) {
                if (stain_db[i].wash_temp_c < wash_temp)
                    wash_temp = stain_db[i].wash_temp_c;  /* don't exceed stain-safe temp */
                if (stain_db[i].recommended_cycle != 0)
                    cycle = stain_db[i].recommended_cycle;
                if (stain_db[i].detergent_ml > det_ml)
                    det_ml = stain_db[i].detergent_ml;  /* more detergent for stains */
                break;
            }
        }

        /* Display result */
        display_scan_result(fabric, stain, fab_conf, wash_temp, cycle, det_ml);

        /* Build and send scan result to hub */
        scan_result_payload_t result;
        memset(&result, 0, sizeof(result));
        result.fabric_type = fabric;
        result.fabric_conf = fab_conf;
        result.stain_type = stain;
        result.stain_conf = stain_conf;
        result.wash_temp_c_x10 = (int16_t)(wash_temp * 10);
        result.recommended_cycle = cycle;
        result.detergent_ml = det_ml;
        result.pre_treat_id = stain;
        memcpy(result.care_label, care_labels, 8);
        result.image_id = 0;  /* cloud assigns */
        result.battery_pct = read_battery_pct();
        result.signal_rssi = 0;

        send_scan_result(&result);

        ESP_LOGI(TAG, "=== SCAN COMPLETE ===");
        ESP_LOGI(TAG, "Going to sleep. Press SCAN for next garment.");

        /* Short delay before allowing next scan */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    /* Initialize GPIOs for LEDs */
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << PIN_LED_WHITE) | (1ULL << PIN_LED_UV) | (1ULL << PIN_LED_IR),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_cfg);
    gpio_set_level(PIN_LED_WHITE, 0);
    gpio_set_level(PIN_LED_UV, 0);
    gpio_set_level(PIN_LED_IR, 0);

    /* Initialize buttons (with pullup) */
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << PIN_BTN_SCAN) | (1ULL << PIN_BTN_NAV) | (1ULL << PIN_BTN_CONFIRM),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn_cfg);

    /* Initialize ADC for battery */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_BATTERY, ADC_ATTEN_DB_11);

    /* Initialize peripherals */
    camera_init();
    display_init();
    sx1261_init();

    /* Start scan task */
    xTaskCreate(scan_task, "scan_task", 16384, NULL, 5, NULL);
}