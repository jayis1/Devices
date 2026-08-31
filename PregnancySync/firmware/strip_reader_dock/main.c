#include <stdbool.h>
#include <stdio.h>

typedef struct {
    const char *protein_level;
    const char *glucose_level;
    const char *ketone_level;
    float specific_gravity;
    bool nitrite_positive;
} strip_result_t;

static strip_result_t classify_strip(float green, float red, float nir) {
    strip_result_t out = {"negative", "negative", "negative", 1.018f, false};
    if (green < 0.42f) {
        out.protein_level = "trace";
    }
    if (red > 0.66f) {
        out.ketone_level = "trace";
    }
    out.specific_gravity = 1.012f + (nir * 0.020f);
    return out;
}

int main(void) {
    strip_result_t result = classify_strip(0.40f, 0.71f, 0.55f);
    printf("strip protein=%s ketone=%s sg=%.3f\n",
           result.protein_level, result.ketone_level, result.specific_gravity);
    return 0;
}
