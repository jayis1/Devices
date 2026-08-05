/* SeizureSync — Prone detection stubs */
#include "prone.h"
#include <string.h>
void mlx90640_get_frame(float *frame) { memset(frame, 0, 768*sizeof(float)); }
int  prone_detect(const float *f) { (void)f; return 0; }