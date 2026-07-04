#ifndef GLUCOSYNC_ACTIVITY_IMU_H
#define GLUCOSYNC_ACTIVITY_IMU_H

#include <stdint.h>

typedef struct {
    uint8_t class_id;    /* 0=sedentary,1=walk,2=run,3=bike,4=strength */
    uint8_t intensity;    /* 0-100 */
    uint8_t confidence;   /* 0-100 */
} activity_result_t;

void activity_imu_init(void);
void activity_imu_set_sample_rate(uint8_t hz);
void activity_imu_read_and_classify(activity_result_t *result);

#endif