/* SeizureSync — SpO2 + apnea (MAX30102) driver header */
#ifndef SPO2_H
#define SPO2_H
float max30102_read_spo2(void);
uint8_t max30102_read_hr(void);
#endif