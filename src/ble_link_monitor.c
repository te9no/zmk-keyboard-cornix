#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/ble.h>
#endif

LOG_MODULE_REGISTER(cornix_ble_link, LOG_LEVEL_INF);

static void link_connected(struct bt_conn *conn, uint8_t err) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("LINK connected peer=%s err=0x%02x", addr, err);

}

static void link_disconnected(struct bt_conn *conn, uint8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("LINK disconnected peer=%s reason=0x%02x", addr, reason);
}

static void link_params(struct bt_conn *conn, uint16_t interval, uint16_t latency,
                        uint16_t timeout) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("LINK params peer=%s interval=%u latency=%u timeout=%u", addr, interval, latency,
            timeout);
}

BT_CONN_CB_DEFINE(cornix_link_callbacks) = {
    .connected = link_connected,
    .disconnected = link_disconnected,
    .le_param_updated = link_params,
};

struct connection_snapshot {
    size_t count;
    size_t central_count;
    size_t peripheral_count;
};

static void log_connection(struct bt_conn *conn, void *user_data) {
    struct connection_snapshot *snapshot = user_data;
    struct bt_conn_info info;
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    int err = bt_conn_get_info(conn, &info);
    if (err) {
        LOG_WRN("LINK detail peer=%s info_err=%d", addr, err);
        snapshot->count++;
        return;
    }

    snapshot->count++;
    if (info.role == BT_CONN_ROLE_CENTRAL) {
        snapshot->central_count++;
    } else {
        snapshot->peripheral_count++;
    }

    LOG_INF("LINK detail peer=%s local_role=%s state=%u security=L%u", addr,
            info.role == BT_CONN_ROLE_CENTRAL ? "central" : "peripheral", info.state,
            bt_conn_get_security(conn));
}

static void log_identity_once(void) {
    static bool logged;
    bt_addr_le_t identities[CONFIG_BT_ID_MAX];
    size_t identity_count = ARRAY_SIZE(identities);

    if (logged) {
        return;
    }

    bt_id_get(identities, &identity_count);
    if (identity_count == 0) {
        LOG_WRN("LINK identity unavailable count=0");
        return;
    }

    for (size_t i = 0; i < identity_count; i++) {
        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(&identities[i], addr, sizeof(addr));
        LOG_INF("LINK identity id=%u addr=%s", (unsigned int)i, addr);
    }
    logged = true;
}

static void link_status_work_handler(struct k_work *work) {
    struct connection_snapshot snapshot = {0};

    log_identity_once();
    bt_conn_foreach(BT_CONN_TYPE_LE, log_connection, &snapshot);
    LOG_INF("LINK status role=%s connections=%u local_central=%u local_peripheral=%u",
            IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) ? "central" : "peripheral",
            (unsigned int)snapshot.count, (unsigned int)snapshot.central_count,
            (unsigned int)snapshot.peripheral_count);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    for (uint8_t i = 0; i < ZMK_BLE_PROFILE_COUNT; i++) {
        bt_addr_le_t *peer = zmk_ble_profile_address(i);
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(peer, addr, sizeof(addr));
        LOG_INF("LINK host_profile index=%u active=%u open=%u connected=%u peer=%s", i,
                i == zmk_ble_active_profile_index(), zmk_ble_profile_is_open(i),
                zmk_ble_profile_is_connected(i), addr);
    }
#endif
    k_work_reschedule(k_work_delayable_from_work(work), K_SECONDS(5));
}

K_WORK_DELAYABLE_DEFINE(link_status_work, link_status_work_handler);

static int link_status_init(void) {
    k_work_schedule(&link_status_work, K_SECONDS(2));
    return 0;
}

SYS_INIT(link_status_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
