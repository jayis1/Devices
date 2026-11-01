#include "protocol.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int ps_validate(const char *s, ps_message *out) {
    if (!s || !out || strlen(s) > PS_MAX_PAYLOAD) return 0;
    unsigned v, seq; unsigned long long ts; char id[33], node[16], kind[16], unit[16]; double value;
    int n = sscanf(s, "{\"v\":%u,\"id\":\"%32[a-zA-Z0-9-]\",\"node\":\"%15[a-zA-Z]\",\"seq\":%u,\"ts_ms\":%llu,\"kind\":\"%15[a-zA-Z_]\",\"value\":%lf,\"unit\":\"%15[a-zA-Z%]\"}", &v,id,node,&seq,&ts,kind,&value,unit);
    if (n != 8 || v != 1 || !isfinite(value) || value < 0 || value > 1000000) return 0;
    if (strcmp(node,"rack") && strcmp(node,"dock") && strcmp(node,"inspection")) return 0;
    if (strcmp(kind,"stock") && strcmp(kind,"return_fill") && strcmp(kind,"inspection")) return 0;
    out->version=v; out->seq=seq; out->ts_ms=ts; out->value=value;
    return 1;
}
