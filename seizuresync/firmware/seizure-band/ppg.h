/* SeizureSync — MAX30102 PPG driver (header) */
#ifndef PPG_H
#define PPG_H
void  ppg_init(int scl, int sda);
float ppg_read_hr(void);    /* heart rate BPM */
int   ppg_read_spo2(void);
#endif