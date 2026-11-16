#include "protocol.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char event[] = "{\"v\":1,\"node\":\"sentinel-01\",\"seq\":1,\"ts\":1730000000,\"kind\":\"environment\",\"mass_g\":0}";
    if (!gl_valid_event(event, strlen(event))) return 1;
    puts(event);
    return 0;
}
