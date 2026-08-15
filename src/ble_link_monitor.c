#include <errno.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/ble.h>
#endif

#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/split/bluetooth/uuid.h>
#endif

LOG_MODULE_REGISTER(cornix_ble_link, LOG_LEVEL_INF);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static const struct bt_data recovery_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_SOME, 0x0f, 0x18),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, ZMK_SPLIT_BT_SERVICE_UUID),
};

static void find_bond(const struct bt_bond_info *info, void *user_data) {
    bt_addr_le_t *central = user_data;
    if (bt_addr_le_cmp(&info->addr, BT_ADDR_LE_NONE) != 0) {
        bt_addr_le_copy(central, &info->addr);
    }
}

static void recover_advertising(struct k_work *work) {
    bt_addr_le_t central = bt_addr_le_none;
    bt_foreach_bond(BT_ID_DEFAULT, find_bond, &central);

    int stop_err = bt_le_adv_stop();
    if (stop_err < 0 && stop_err != -EALREADY) {
        LOG_WRN("ADV recovery stop failed: %d", stop_err);
    }

    int err;
    if (bt_addr_le_cmp(&central, BT_ADDR_LE_NONE) != 0) {
        const struct bt_le_adv_param param = *BT_LE_ADV_CONN_DIR_LOW_DUTY(&central);
        err = bt_le_adv_start(&param, NULL, 0, NULL, 0);
    } else {
        err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, recovery_ad, ARRAY_SIZE(recovery_ad), NULL, 0);
    }

    if (err == -EALREADY) {
        LOG_WRN("ADV recovery still busy; retrying");
        k_work_reschedule(k_work_delayable_from_work(work), K_MSEC(500));
    } else if (err < 0) {
        LOG_ERR("ADV recovery failed: %d", err);
        k_work_reschedule(k_work_delayable_from_work(work), K_SECONDS(1));
    } else {
        LOG_INF("ADV recovery started");
    }
}

K_WORK_DELAYABLE_DEFINE(advertising_recovery_work, recover_advertising);

#endif

static void link_connected(struct bt_conn *conn, uint8_t err) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("LINK connected peer=%s err=0x%02x", addr, err);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    if (err == 0) {
        k_work_cancel_delayable(&advertising_recovery_work);
    } else if (err == BT_HCI_ERR_ADV_TIMEOUT) {
        LOG_WRN("Directed advertising timed out; scheduling recovery");
        k_work_reschedule(&advertising_recovery_work, K_MSEC(500));
    }
#endif
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
