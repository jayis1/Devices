/* SeizureSync — ICM-42688-P accelerometer driver (header) */
#ifndef ACCEL_H
#define ACCEL_H
void  accel_init(int cs, int sck, int miso, int mosi, int int1);
float accel_read_magnitude(void);  /* sqrt(x²+y²+z²) in g */
#endif