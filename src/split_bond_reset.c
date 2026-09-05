#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(cornix_split_reset, LOG_LEVEL_INF);

struct saved_split {
    int index;
    bt_addr_le_t address;
};

struct saved_split_context {
    struct saved_split splits[CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS];
    size_t count;
};

struct connected_split_context {
    bt_addr_le_t addresses[CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS];
    size_t count;
};

static int load_peripheral_address(const char *key, size_t len, settings_read_cb read_cb,
                                   void *cb_arg, void *param) {
    struct saved_split_context *context = param;
    char *end;
    long index = strtol(key, &end, 10);

    if (end == key || *end != '\0' || index < 0 ||
        index >= CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS) {
        LOG_WRN("Ignoring invalid split address setting %s", key);
        return 0;
    }
    if (context->count >= ARRAY_SIZE(context->splits)) {
        return 0;
    }

    if (len != sizeof(bt_addr_le_t)) {
        LOG_WRN("Ignoring split address setting %s with unexpected size %u", key,
                (unsigned int)len);
        return 0;
    }

    struct saved_split *split = &context->splits[context->count];
    ssize_t read = read_cb(cb_arg, &split->address, sizeof(split->address));
    if (read < 0) {
        LOG_ERR("Failed to read split address setting %s: %d", key, (int)read);
        return 0;
    }
    if (read != sizeof(split->address)) {
        LOG_WRN("Ignoring truncated split address setting %s: %d", key, (int)read);
        return 0;
    }

    split->index = (int)index;
    context->count++;
    return 0;
}

static void collect_connected_split(struct bt_conn *conn, void *param) {
    struct connected_split_context *context = param;
    struct bt_conn_info info;

    if (context->count >= ARRAY_SIZE(context->addresses) || bt_conn_get_info(conn, &info) != 0 ||
        info.role != BT_CONN_ROLE_CENTRAL || bt_conn_get_security(conn) < BT_SECURITY_L2) {
        return;
    }

    bt_addr_le_copy(&context->addresses[context->count], bt_conn_get_dst(conn));
    context->count++;
}

static bool split_is_connected(const struct connected_split_context *context,
                               const bt_addr_le_t *address) {
    for (size_t i = 0; i < context->count; i++) {
        if (bt_addr_le_cmp(&context->addresses[i], address) == 0) {
            return true;
        }
    }
    return false;
}

static void clear_split_bonds_handler(struct k_work *work) {
    ARG_UNUSED(work);

    struct saved_split_context saved = {0};
    struct connected_split_context connected = {0};
    int err = settings_load_subtree_direct("ble/peripheral_addresses", load_peripheral_address,
                                           &saved);
    if (err) {
        LOG_ERR("Failed to load saved split peripheral addresses: %d", err);
        return;
    }

    bt_conn_foreach(BT_CONN_TYPE_LE, collect_connected_split, &connected);

    size_t removed = 0;
    for (size_t i = 0; i < saved.count; i++) {
        const struct saved_split *split = &saved.splits[i];
        char address[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(&split->address, address, sizeof(address));

        if (split_is_connected(&connected, &split->address)) {
            LOG_INF("Preserved connected split slot %d for %s", split->index, address);
            continue;
        }

        err = bt_unpair(BT_ID_DEFAULT, &split->address);
        if (err && err != -ENOENT) {
            LOG_ERR("Failed to remove split bond for %s: %d", address, err);
        } else {
            LOG_INF("Removed split bond for %s", address);
        }

        char setting_name[40];
        snprintf(setting_name, sizeof(setting_name), "ble/peripheral_addresses/%d", split->index);

        err = settings_delete(setting_name);
        if (err && err != -ENOENT) {
            LOG_ERR("Failed to delete %s: %d", setting_name, err);
        } else {
            removed++;
        }
    }

    LOG_WRN("Disconnected split cleanup complete: removed=%u preserved=%u; host profiles preserved",
            (unsigned int)removed, (unsigned int)connected.count);
}

K_WORK_DELAYABLE_DEFINE(clear_split_bonds_work, clear_split_bonds_handler);

static int clear_split_bonds_init(void) {
    k_work_schedule(&clear_split_bonds_work,
                    K_MSEC(CONFIG_CORNIX_SPLIT_BOND_RESET_DELAY_MS));
    return 0;
}

SYS_INIT(clear_split_bonds_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
