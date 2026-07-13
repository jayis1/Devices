/*
 * max30102.c — MAX30102 PPG sensor driver for nRF52833
 *
 * Configures the MAX30102 for PPG (green LED for HR, red+IR for SpO₂)
 * at 100 Hz sample rate, 18-bit resolution, with 0.4 mA LED current.
 *
 * License: MIT
 */
#include "max30102.h"
#include <string.h>

/* I²C abstraction (nRF52833) */
static void i2c_write(uint8_t reg, uint8_t val)
{
    /* nrf_drv_twi_tx to MAX30102_I2C_ADDR with [reg, val] */
}

static uint8_t i2c_read(uint8_t reg)
{
    /* nrf_drv_twi_tx reg → nrf_drv_twi_rx → return byte */
    return 0;
}

static void i2c_read_burst(uint8_t reg, uint8_t *buf, int len)
{
    /* nrf_drv_twi_tx reg → nrf_drv_twi_rx buf */
}

void max_init(void)
{
    /* Reset device */
    i2c_write(MAX_REG_MODE_CONFIG, MAX_MODE_RESET);
    /* Delay 10 ms for reset */
    /* In production: nrf_delay_ms(10) */

    /* Clear interrupts */
    i2c_write(MAX_REG_INTR_STATUS_1, 0x00);
    i2c_write(MAX_REG_INTR_STATUS_2, 0x00);

    /* Enable data ready interrupt */
    i2c_write(MAX_REG_INTR_ENABLE_1, 0x80);  /* PPG_RDY_EN */

    /* FIFO config: 16 samples average, no rollover, 1 sample */
    i2c_write(MAX_REG_FIFO_CONFIG, 0x4F);

    /* Mode: SpO2 (red + IR) for SpO2, or MULTI for green+red+IR */
    /* Use MULTI_LED mode for green + red + IR */
    i2c_write(MAX_REG_MODE_CONFIG, MAX_MODE_MULTI_LED);

    /* SPO2 config: 100 Hz sample rate, 411 μs pulse width (18-bit) */
    i2c_write(MAX_REG_SPO2_CONFIG, 0x27);  /* SR=100Hz, PW=411μs */

    /* LED amplitudes (pulse current) */
    /* Red: 0x0F (3.0 mA), IR: 0x0F (3.0 mA), Green: 0x0F (3.0 mA) */
    i2c_write(MAX_REG_LED1_PA, 0x0F);   /* Red */
    i2c_write(MAX_REG_LED2_PA, 0x0F);   /* IR */
    /* Green is on LED3/LED4 via multi-LED mode */
    i2c_write(MAX_REG_PILOT_PA, 0x0F);  /* Pilot current */

    /* Multi-LED mode control: Slot1=Green, Slot2=Red, Slot3=IR */
    i2c_write(MAX_REG_MULTI_LED_CTRL1, 0x21);  /* Slot1=Green(2), Slot2=Red(1) */
    i2c_write(MAX_REG_MULTI_LED_CTRL2, 0x03);  /* Slot3=IR(3) */
}

void max_reset(void)
{
    i2c_write(MAX_REG_MODE_CONFIG, MAX_MODE_RESET);
    /* delay 10ms */
}

void max_set_mode(uint8_t mode)
{
    i2c_write(MAX_REG_MODE_CONFIG, mode);
}

void max_set_sample_rate(uint8_t sr, uint8_t led_pw)
{
    /* sr: 0=50Hz, 1=100Hz, 2=200Hz, 3=400Hz */
    /* led_pw: 0=69μs(15bit), 1=118μs(16bit), 2=215μs(17bit), 3=411μs(18bit) */
    uint8_t config = (sr << 5) | (led_pw << 2);
    i2c_write(MAX_REG_SPO2_CONFIG, config);
}

void set_led_amplitude(uint8_t red_pa, uint8_t ir_pa, uint8_t green_pa)
{
    i2c_write(MAX_REG_LED1_PA, red_pa);
    i2c_write(MAX_REG_LED2_PA, ir_pa);
    i2c_write(MAX_REG_PILOT_PA, green_pa);
}

int max_read_fifo(max_sample_t *samples, int max_count)
{
    /* Read FIFO write and read pointers to determine available samples */
    uint8_t wr_ptr = i2c_read(MAX_REG_FIFO_WR_PTR);
    uint8_t rd_ptr = i2c_read(MAX_REG_FIFO_RD_PTR);
    int num_samples = (wr_ptr - rd_ptr) & 0x1F;  /* 32-entry FIFO */

    if (num_samples > max_count) num_samples = max_count;

    for (int i = 0; i < num_samples; i++) {
        /* Each sample = 9 bytes (3 channels × 3 bytes, 18-bit each) */
        uint8_t buf[9];
        i2c_read_burst(MAX_REG_FIFO_DATA, buf, 9);

        /* Green (slot 1) — bytes 0-2 */
        samples[i].green = ((uint32_t)(buf[0] & 0x03) << 16)
                          | ((uint32_t)buf[1] << 8) | buf[2];
        /* Red (slot 2) — bytes 3-5 */
        samples[i].red = ((uint32_t)(buf[3] & 0x03) << 16)
                        | ((uint32_t)buf[4] << 8) | buf[5];
        /* IR (slot 3) — bytes 6-8 */
        samples[i].ir = ((uint32_t)(buf[6] & 0x03) << 16)
                       | ((uint32_t)buf[7] << 8) | buf[8];
    }

    return num_samples;
}

uint8_t max_read_intr(void)
{
    return i2c_read(MAX_REG_INTR_STATUS_1);
}

void max_clear_intr(void)
{
    i2c_write(MAX_REG_INTR_STATUS_1, 0x00);
    i2c_write(MAX_REG_INTR_STATUS_2, 0x00);
}

float max_read_temperature(void)
{
    /* Enable temperature */
    i2c_write(MAX_REG_TEMP_CONFIG, 0x01);
    /* Wait for conversion */
    /* Read integer + fractional */
    uint8_t intr = i2c_read(MAX_REG_TEMP_INTR);
    uint8_t frac = i2c_read(MAX_REG_TEMP_FRAC);
    float temp = (float)intr + (float)(frac & 0x0F) * 0.0625f;
    return temp;
}