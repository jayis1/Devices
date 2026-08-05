/* SeizureSync — Prone position detection (MLX90640) header */
#ifndef PRONE_H
#define PRONE_H
void mlx90640_get_frame(float *frame);  /* 32x24 = 768 floats */
int  prone_detect(const float *frame);
#endif