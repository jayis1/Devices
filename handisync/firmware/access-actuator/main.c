#include <stdbool.h>
#include <stdint.h>
void actuator_move(bool extend) { uint32_t start=millis(); if(estop_active() || limit_for_direction(extend)) return; motor_set(extend); while(!limit_for_direction(extend) && !estop_active()) { if(motor_current_ma()>3500 || millis()-start>12000 || !hall_progressed_within(500)) { motor_off(); fault_latch(); return; } } motor_off(); }
