/*
 * OralSync Saliva Sensor — main firmware (STM32L432KC + SPBTLE-RF BLE)
 *
 * Daily salivary biochemistry: pH (ISFET), nitrite (amperometry), buffer
 * capacity (HX711 titration micro-volume). Reports one OSMP_SALIVA_READING
 * per day over BLE. CR2477 coin-cell, ~6-month battery.
 *
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "osmp.h"
#include "osmp_sensors.h"

static uint8_t s_seq = 0;
static uint8_t s_tx[OSMT_MAX_FRAME];
static uint8_t s_rx[OSMT_MAX_FRAME];

extern void ble_init(void);
extern void ble_send(const uint8_t *frame, size_t len);
extern int  ble_recv(uint8_t *out, size_t cap, uint32_t timeout_ms);
extern void ble_connect_hub(void);
extern void ble_disconnect(void);

static void send_frame(size_t len) { ble_send(s_tx, len); }

/*
 * Buffer capacity: titrate a known acid micro-volume into the saliva sample
 * and measure mass added (HX711) until pH crosses 5.5. Higher mass = higher
 * buffer capacity (good). Returns a 0..5 index.
 */
static uint8_t measure_buffer_capacity(uint16_t ph0_x100)
{
    /* Simplified: in production this drives a micro-peristaltic pump.
     * Here we approximate from initial pH and titration slope. */
    if (ph0_x100 >= 700) return 5;   /* alkaline, well-buffered */
    if (ph0_x100 >= 680) return 4;
    if (ph0_x100 >= 660) return 3;
    if (ph0_x100 >= 640) return 2;
    if (ph0_x100 >= 620) return 1;
    return 0;                         /* acidic, poorly buffered */
}

static void take_reading(void)
{
    /* Power up sensors */
    ph_init();
    nitrite_init();
    ds18b20_init();
    hx711_init();

    /* Temperature-compensated pH */
    int16_t temp_c10 = ds18b20_read_c10();
    uint16_t ph_x100 = ph_read_x100();
    /* Temp compensation: pH(T) = pH(25) + 0.0025*(25-T) */
    int16_t delta = (int16_t)((250 - temp_c10) * 0.025f * 10);
    ph_x100 = (uint16_t)((int16_t)ph_x100 + delta);

    uint16_t nitrite_um = nitrite_read_um();
    uint8_t  buffer = measure_buffer_capacity(ph_x100);

    /* Connect to Hub and uplink */
    ble_connect_hub();

    /* HELLO */
    size_t n = osmp_build_hello(s_tx, sizeof(s_tx), OSMP_NODE_SALIVA,
                                /*hw*/1, /*fw*/0x0100, /*caps*/0x0001);
    send_frame(n);

    /* SALIVA_READING */
    n = osmp_build_saliva(s_tx, sizeof(s_tx), s_seq++,
                          ph_x100, nitrite_um, buffer, (uint16_t)temp_c10);
    send_frame(n);

    /* Wait for ACK (best-effort) */
    int rlen = ble_recv(s_rx, sizeof(s_rx), 3000 /*ms*/);
    if (rlen > 0) {
        uint8_t type, seq, plen, payload[OSMP_MAX_PAYLOAD];
        if (osmp_decode(s_rx, (size_t)rlen, &type, &seq, payload, &plen)
            && type == OSMP_ACK) {
            /* OK — reading acknowledged */
        }
    }

    ble_disconnect();
}

int main(void)
{
    ble_init();

    /* RTC alarm wakes once per day at user-configured time (default 08:00).
     * Between readings: STOP2 mode, ~3 µA, ~6 months on CR2477. */
    while (1) {
        take_reading();
        /* Enter STOP2 until next RTC alarm.
         * In real firmware: HAL_PWR_EnterSTOPMode(...) then reinit clocks. */
        __asm__ volatile ("wfi");
    }
    return 0;
}

/* RTC alarm ISR — triggers the daily reading. */
void RTC_Alarm_IRQHandler(void)
{
    /* Flag handled in main loop; just clear the alarm flag. */
}