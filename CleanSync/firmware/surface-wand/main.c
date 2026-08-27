#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

static const char *class_name(uint8_t residue_class) {
    switch (residue_class) {
        case 0: return "clean";
        case 1: return "dust";
        case 2: return "soap";
        case 3: return "grease";
        case 4: return "biofilm";
        case 5: return "mildew-risk";
        default: return "unknown";
    }
}

int main(void) {
    cs_wand_scan_t scan = {
        .mode = 1u,
        .residue_class = 3u,
        .confidence_pct = 91u,
        .fluorescence_score = 74u,
        .battery_mv = 3890u,
        .image_tag = 2026u,
    };

    printf("wand class=%s confidence=%u fluorescence=%u battery=%u\n",
           class_name(scan.residue_class),
           scan.confidence_pct,
           scan.fluorescence_score,
           scan.battery_mv);
    return 0;
}
