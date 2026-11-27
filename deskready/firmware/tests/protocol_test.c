#include "../common/event.h"
#include <assert.h>
#include <string.h>
int main(void) { dr_event_t e={1,"desk-01",7,"checkin",true,120}; char b[160]; size_t n=dr_encode(&e,b,sizeof b); assert(n>0); assert(strstr(b,"\"ready\":true")); assert(dr_encode(&e,b,8)==0); e.version=2; assert(dr_encode(&e,b,sizeof b)==0); return 0; }
