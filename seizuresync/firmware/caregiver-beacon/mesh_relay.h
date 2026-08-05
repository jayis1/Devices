/* SeizureSync — Caregiver Beacon mesh relay (header) */
#ifndef MESH_RELAY_H
#define MESH_RELAY_H
void mesh_relay_init(void);
int  mesh_relay_packet(const uint8_t *data, size_t len);
#endif