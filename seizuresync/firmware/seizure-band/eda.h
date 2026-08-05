/* SeizureSync — AD5940 EDA driver (header) */
#ifndef EDA_H
#define EDA_H
void  eda_init(int scl, int sda);
float eda_read_microsiemens(void);
#endif