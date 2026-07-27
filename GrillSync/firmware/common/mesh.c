/*
 * GrillSync — TDMA Mesh Networking Layer
 */
#include "mesh.h"
#include "config.h"

int gs_mesh_init(gs_mesh_ctx_t *mesh, uint8_t node_type,
                 const gs_spi_interface_t *spi,
                 const gs_radio_config_t *radio_cfg)
{
    memset(mesh, 0, sizeof(*mesh));
    mesh->node_type = node_type;
    mesh->msg_seq = 0;
    mesh->joined = 0;

    int rc = gs_radio_init(&mesh->radio, spi, radio_cfg);
    if (rc != 0)
        return rc;

    if (node_type == GS_NODE_HUB) {
        mesh->node_id = GS_HUB_NODE_ID;
        mesh->tdma_slot = 0;
        mesh->joined = 1;
    } else {
        mesh->node_id = 0xFF;  /* Unassigned */
    }

    return 0;
}

int gs_mesh_send(gs_mesh_ctx_t *mesh, const gs_message_t *msg)
{
    uint8_t buf[GS_MAX_MSG];
    size_t len = gs_encode(msg, buf, sizeof(buf));
    if (len == 0)
        return -1;
    return gs_radio_tx(&mesh->radio, buf, (uint8_t)len);
}

int gs_mesh_recv(gs_mesh_ctx_t *mesh, gs_message_t *msg, uint32_t timeout_ms)
{
    uint8_t buf[GS_MAX_MSG];
    int len = gs_radio_rx(&mesh->radio, buf, sizeof(buf), timeout_ms);
    if (len <= 0)
        return -1;
    return gs_decode(msg, buf, (size_t)len);
}

int gs_mesh_hub_assign_slot(gs_mesh_ctx_t *mesh, uint8_t node_type,
                             uint8_t *node_id, uint8_t *slot)
{
    /* Find free slot (1..GS_SLOT_COUNT-1, 0 = hub) */
    for (uint8_t i = 1; i < GS_SLOT_COUNT; i++) {
        if (!mesh->slot_assigned[i]) {
            mesh->slot_assigned[i] = 1;
            mesh->slot_node_type[i] = node_type;
            *node_id = i;
            *slot = i;
            return 0;
        }
    }
    return -1;
}

void gs_mesh_hub_time_sync(gs_mesh_ctx_t *mesh, uint32_t epoch)
{
    gs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.sync[0] = GS_SYNC0;
    msg.header.sync[1] = GS_SYNC1;
    msg.header.src = GS_HUB_NODE_ID;
    msg.header.dst = GS_BROADCAST;
    msg.header.type = GS_MSG_TIME_SYNC;
    msg.header.msg_id = mesh->msg_seq++;

    msg.payload[0] = (uint8_t)(epoch & 0xFF);
    msg.payload[1] = (uint8_t)(epoch >> 8);
    msg.payload[2] = (uint8_t)(epoch >> 16);
    msg.payload[3] = (uint8_t)(epoch >> 24);
    msg.payload_len = 4;

    gs_mesh_send(mesh, &msg);
    mesh->last_time_sync = epoch;
}

int gs_mesh_join(gs_mesh_ctx_t *mesh, uint8_t node_type, uint8_t battery_mv,
                 uint8_t fw_version)
{
    gs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.sync[0] = GS_SYNC0;
    msg.header.sync[1] = GS_SYNC1;
    msg.header.src = 0xFF;  /* Unassigned */
    msg.header.dst = GS_HUB_NODE_ID;
    msg.header.type = GS_MSG_JOIN_REQ;
    msg.header.msg_id = mesh->msg_seq++;

    msg.payload[0] = node_type;
    msg.payload[1] = battery_mv;
    msg.payload[2] = fw_version;
    msg.payload_len = 3;

    return gs_mesh_send(mesh, &msg);
}

int gs_mesh_heartbeat(gs_mesh_ctx_t *mesh, uint8_t battery_mv, int8_t rssi)
{
    gs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.sync[0] = GS_SYNC0;
    msg.header.sync[1] = GS_SYNC1;
    msg.header.src = mesh->node_id;
    msg.header.dst = GS_HUB_NODE_ID;
    msg.header.type = GS_MSG_HEARTBEAT;
    msg.header.msg_id = mesh->msg_seq++;

    msg.payload[0] = battery_mv;
    msg.payload[1] = (uint8_t)rssi;
    msg.payload_len = 2;

    return gs_mesh_send(mesh, &msg);
}