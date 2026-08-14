/*
 * Madgwick AHRS implementation for Spine Band
 * (Included in main.c — this file provides the standalone filter)
 */

#include "madgwick_ahrs.h"
#include <math.h>

void madgwick_init(madgwick_state_t *s, float beta, float sample_dt)
{
    s->q0 = 1.0f;
    s->q1 = 0.0f;
    s->q2 = 0.0f;
    s->q3 = 0.0f;
    s->beta = beta;
    s->sample_dt = sample_dt;
}

void madgwick_update(madgwick_state_t *s,
                     float gx, float gy, float gz,
                     float ax, float ay, float az)
{
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2, _4qb3;
    float _8q1, _8q2, q0q0, q1q1, q2q2, q3q3;

    qDot1 = 0.5f * (-s->q1 * gx - s->q2 * gy - s->q3 * gz);
    qDot2 = 0.5f * ( s->q0 * gx + s->q2 * gz - s->q3 * gy);
    qDot3 = 0.5f * ( s->q0 * gy - s->q1 * gz + s->q3 * gx);
    qDot4 = 0.5f * ( s->q0 * gz + s->q1 * gy - s->q2 * gx);

    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
        recipNorm = 1.0f / sqrtf(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        _2q0 = 2.0f * s->q0;
        _2q1 = 2.0f * s->q1;
        _2q2 = 2.0f * s->q2;
        _2q3 = 2.0f * s->q3;
        _4q0 = 4.0f * s->q0;
        _4q1 = 4.0f * s->q1;
        _4q2 = 4.0f * s->q2;
        _8q1 = 8.0f * s->q1;
        _8q2 = 8.0f * s->q2;
        q0q0 = s->q0 * s->q0;
        q1q1 = s->q1 * s->q1;
        q2q2 = s->q2 * s->q2;
        q3q3 = s->q3 * s->q3;

        s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;

        recipNorm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;

        qDot1 -= s->beta * s0;
        qDot2 -= s->beta * s1;
        qDot3 -= s->beta * s2;
        qDot4 -= s->beta * s3;
    }

    s->q0 += qDot1 * s->sample_dt;
    s->q1 += qDot2 * s->sample_dt;
    s->q2 += qDot3 * s->sample_dt;
    s->q3 += qDot4 * s->sample_dt;

    recipNorm = 1.0f / sqrtf(s->q0 * s->q0 + s->q1 * s->q1 + s->q2 * s->q2 + s->q3 * s->q3);
    s->q0 *= recipNorm;
    s->q1 *= recipNorm;
    s->q2 *= recipNorm;
    s->q3 *= recipNorm;
}

void madgwick_get_euler(const madgwick_state_t *s, float *pitch, float *roll, float *yaw)
{
    *roll  = atan2f(2.0f * (s->q0 * s->q1 + s->q2 * s->q3), 1.0f - 2.0f * (s->q1 * s->q1 + s->q2 * s->q2));
    *pitch = asinf(2.0f * (s->q0 * s->q2 - s->q3 * s->q1));
    *yaw   = atan2f(2.0f * (s->q0 * s->q3 + s->q1 * s->q2), 1.0f - 2.0f * (s->q2 * s->q2 + s->q3 * s->q3));

    *pitch *= 180.0f / 3.14159265f;
    *roll  *= 180.0f / 3.14159265f;
    *yaw   *= 180.0f / 3.14159265f;
}