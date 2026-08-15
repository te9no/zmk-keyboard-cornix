#define DT_DRV_COMPAT zmk_cdc_acm_bootloader_trigger

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#if IS_ENABLED(CONFIG_RETENTION_BOOT_MODE)
#include <zephyr/retention/bootmode.h>
#endif

LOG_MODULE_REGISTER(cornix_cdc_boot, CONFIG_ZMK_LOG_LEVEL);

struct trigger_config {
    const struct device *cdc;
};

struct trigger_data {
    bool armed;
    struct k_work_delayable boot_work;
};

static struct trigger_data *active_trigger;

static void enter_bootloader(struct k_work *work) {
    ARG_UNUSED(work);

#if IS_ENABLED(CONFIG_RETENTION_BOOT_MODE)
    int err = bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);
    if (err < 0) {
        LOG_ERR("Failed to set UF2 boot mode: %d", err);
        return;
    }
    sys_reboot(SYS_REBOOT_WARM);
#else
    sys_reboot(0x57);
#endif
}

static void dte_rate_changed(const struct device *dev, uint32_t rate) {
    ARG_UNUSED(dev);

    if (active_trigger == NULL) {
        return;
    }

    if (rate == 1200 && !active_trigger->armed) {
        active_trigger->armed = true;
        LOG_INF("1200-baud CDC callback detected; entering UF2");
        k_work_schedule(&active_trigger->boot_work,
                        K_MSEC(CONFIG_ZMK_CDC_ACM_BOOTLOADER_TRIGGER_DELAY_MS));
    } else if (rate != 1200) {
        active_trigger->armed = false;
    }
}

static int trigger_init(const struct device *dev) {
    const struct trigger_config *config = dev->config;
    struct trigger_data *data = dev->data;

    if (!device_is_ready(config->cdc)) {
        LOG_ERR("CDC ACM device is not ready");
        return -ENODEV;
    }

    k_work_init_delayable(&data->boot_work, enter_bootloader);
    active_trigger = data;

    int err = cdc_acm_dte_rate_callback_set(config->cdc, dte_rate_changed);
    if (err < 0) {
        active_trigger = NULL;
        LOG_ERR("Failed to register CDC rate callback: %d", err);
        return err;
    }

    LOG_INF("CDC 1200-baud UF2 trigger ready");
    return 0;
}

#define TRIGGER_DEFINE(n)                                                                    \
    static struct trigger_data trigger_data_##n;                                             \
    static const struct trigger_config trigger_config_##n = {                                \
        .cdc = DEVICE_DT_GET(DT_INST_PHANDLE(n, cdc_port)),                                  \
    };                                                                                       \
    DEVICE_DT_INST_DEFINE(n, trigger_init, NULL, &trigger_data_##n, &trigger_config_##n,     \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);

DT_INST_FOREACH_STATUS_OKAY(TRIGGER_DEFINE)
