/*
 * SeizureSync — Bed-mat ballistocardiography (BCG) driver
 * SPDX-License-Identifier: MIT
 */
#ifndef BCG_H
#define BCG_H

void    bcg_init(void);
float   bcg_get_breathing_rate(void);   /* breaths per minute */
float   bcg_get_heart_rate(void);       /* BPM from cardiac BCG */
float   bcg_get_motion_energy(void);

#endif