#ifndef GREASELOOP_PROTOCOL_H
#define GREASELOOP_PROTOCOL_H
#include <stddef.h>
#include <string.h>
#define GL_MAX_PAYLOAD 512u
/* Prototype guard only: production must use a real bounded JSON parser and authentication. */
static int gl_valid_event(const char *s, size_t n) {
    if (!s || n == 0 || n > GL_MAX_PAYLOAD || s[0] != '{' || s[n - 1] != '}') return 0;
    if (!strstr(s, "\"v\":1") || !strstr(s, "\"node\":") || !strstr(s, "\"seq\":")) return 0;
    if (!strstr(s, "\"ts\":") || !strstr(s, "\"kind\":")) return 0;
    if (!strstr(s, "\"mass_g\":")) return 0;
    return 1;
}
#endif
