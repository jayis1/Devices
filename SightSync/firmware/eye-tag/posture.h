/**
 * SightSync Eye Tag — Head Posture Detection (IMU)
 *
 * Uses LSM6DSO 6-axis IMU to compute pitch/roll/yaw via
 * complementary filter, and detect forward head posture.
 *
 * License: MIT
 */

#ifndef POSTURE_H
#define POSTURE_H

#include <stdint.h>
#include <Adafruit_LSM6DS.h>

void posture_init(Adafruit_LSM6DS *imu);
void posture_sample(void);
void posture_set_mode(uint8_t mode);
void posture_get_angles(int16_t *pitch, int16_t *roll, int16_t *yaw,
                         uint8_t *forward_flag, uint8_t *risk);

#endif /* POSTURE_H */