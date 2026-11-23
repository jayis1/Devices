#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* Structural/range guard for the constrained prototype. Production must use
 * a bounded JSON parser and authenticated radio framing. */
int umbrella_payload_ok(const char *p, size_t n) {
    if (!p || !n || n > 192) return 0;
    char buf[193];
    memcpy(buf, p, n);
    buf[n] = '\0';
    p = buf;
    if (!strstr(p, "\"v\":1") || !strstr(p, "\"kind\":\"slots\"")) return 0;
    const char *f = strstr(p, "\"free\":");
    const char *t = strstr(p, "\"total\":");
    if (!f || !t) return 0;
    char *end;
    long free_count = strtol(f + 7, &end, 10);
    if (end == f + 7 || free_count < 0 || free_count > 4) return 0;
    long total = strtol(t + 8, &end, 10);
    if (end == t + 8 || total < 0 || total > 4 || free_count > total) return 0;
    return 1;
}
