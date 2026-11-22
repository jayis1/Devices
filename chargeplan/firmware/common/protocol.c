#include <stddef.h>
#include <string.h>

/* Deliberately tiny bounded validator for the documented prototype event envelope.
 * Production must use a real JSON parser and authenticated provisioning. */
int cp_event_valid(const char *s, size_t n) {
    if (!s || n == 0 || n > 512) return 0;
    char buf[513];
    memcpy(buf, s, n);
    buf[n] = '\0';
    if (strstr(buf, "\"v\":1") == NULL) return 0;
    if (strstr(buf, "\"node\":\"") == NULL) return 0;
    if (strstr(buf, "\"seq\":") == NULL || strstr(buf, "\"ts\":") == NULL) return 0;
    if (strstr(buf, "\"kind\":\"session\"") == NULL && strstr(buf, "\"kind\":\"power\"") == NULL) return 0;
    return buf[0] == '{' && buf[n-1] == '}';
}
