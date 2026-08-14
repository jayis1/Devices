#ifndef MADGWICK_AHRS_H
#define MADGWICK_AHRS_H

typedef struct {
    float q0, q1, q2, q3;
    float beta;
    float sample_dt;
} madgwick_state_t;

void madgwick_init(madgwick_state_t *s, float beta, float sample_dt);
void madgwick_update(madgwick_state_t *s, float gx, float gy, float gz,
                     float ax, float ay, float az);
void madgwick_get_euler(const madgwick_state_t *s, float *pitch, float *roll, float *yaw);

#endif