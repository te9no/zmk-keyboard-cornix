#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/ble.h>

LOG_MODULE_REGISTER(cornix_host_reset, LOG_LEVEL_INF);

static void clear_host_bonds_handler(struct k_work *work) {
    ARG_UNUSED(work);

    /*
     * ZMK_BLE_PROFILE_COUNT excludes the slots reserved for split
     * peripherals on a Central. This clears only PC/phone profiles.
     */
    zmk_ble_clear_all_bonds();
    LOG_WRN("Central host profiles cleared; split peripheral bonds preserved");
}

K_WORK_DELAYABLE_DEFINE(clear_host_bonds_work, clear_host_bonds_handler);

static int clear_host_bonds_init(void) {
    k_work_schedule(&clear_host_bonds_work, K_SECONDS(3));
    return 0;
}

SYS_INIT(clear_host_bonds_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
