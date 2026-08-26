#include <errno.h>
#include <stdint.h>

#include <cormoran/pmw3610/pmw3610_api.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cornix_pmw_diag, LOG_LEVEL_INF);

#define PMW3610_REG_PRODUCT_ID 0x00
#define PMW3610_REG_REVISION_ID 0x01
#define PMW3610_REG_MOTION 0x02
#define PMW3610_REG_DELTA_X_L 0x03
#define PMW3610_REG_DELTA_Y_L 0x04
#define PMW3610_REG_DELTA_XY_H 0x05
#define PMW3610_REG_PERFORMANCE 0x11
#define PMW3610_REG_OBSERVATION 0x2d

#define DIAGNOSTIC_FIRST_DELAY K_SECONDS(4)
#define DIAGNOSTIC_INTERVAL K_SECONDS(5)

static const struct gpio_dt_spec motion_irq =
    GPIO_DT_SPEC_GET(DT_NODELABEL(madula_trackball), irq_gpios);

static int read_motion_registers(const struct device *dev, uint8_t *motion, int16_t *x,
                                 int16_t *y) {
    uint8_t x_low = 0;
    uint8_t y_low = 0;
    uint8_t xy_high = 0;
    int err;

    err = pmw3610_read_register(dev, PMW3610_REG_MOTION, motion);
    if (err) {
        return err;
    }
    err = pmw3610_read_register(dev, PMW3610_REG_DELTA_X_L, &x_low);
    if (err) {
        return err;
    }
    err = pmw3610_read_register(dev, PMW3610_REG_DELTA_Y_L, &y_low);
    if (err) {
        return err;
    }
    err = pmw3610_read_register(dev, PMW3610_REG_DELTA_XY_H, &xy_high);
    if (err) {
        return err;
    }

    *x = (int16_t)((((uint16_t)xy_high & 0xf0U) << 4) | x_low);
    *y = (int16_t)((((uint16_t)xy_high & 0x0fU) << 8) | y_low);
    if (*x & 0x0800) {
        *x |= (int16_t)0xf000;
    }
    if (*y & 0x0800) {
        *y |= (int16_t)0xf000;
    }

    return 0;
}

static void log_register(const struct device *dev, const char *name, uint8_t address) {
    uint8_t value = 0;
    int err = pmw3610_read_register(dev, address, &value);

    if (err) {
        LOG_ERR("PMW_DIAG reg=%s address=0x%02x read_err=%d", name, address, err);
    } else {
        LOG_INF("PMW_DIAG reg=%s address=0x%02x value=0x%02x", name, address, value);
    }
}

static void diagnostic_work_handler(struct k_work *work) {
    size_t count = pmw3610_device_count();

    LOG_INF("PMW_DIAG devices=%u irq_port_ready=%u", (unsigned int)count,
            device_is_ready(motion_irq.port));

    if (device_is_ready(motion_irq.port)) {
        int irq_raw = gpio_pin_get_raw(motion_irq.port, motion_irq.pin);
        int irq_asserted = gpio_pin_get_dt(&motion_irq);
        LOG_INF("PMW_DIAG irq=%s/%u flags=0x%x raw=%d asserted=%d", motion_irq.port->name,
                motion_irq.pin, motion_irq.dt_flags, irq_raw, irq_asserted);
    }

    if (count == 0) {
        LOG_ERR("PMW_DIAG no PMW3610 devicetree instances");
        goto reschedule;
    }

    const struct device *dev = pmw3610_get_device(0);
    bool ready = pmw3610_is_ready(dev);
    int init_err = pmw3610_get_init_error(dev);
    struct pmw3610_runtime_config runtime = {0};
    int runtime_err = pmw3610_get_runtime_config(dev, &runtime);

    LOG_INF("PMW_DIAG device=%s zephyr_ready=%u async_ready=%u init_err=%d runtime_err=%d",
            dev->name, device_is_ready(dev), ready, init_err, runtime_err);
    if (!runtime_err) {
        LOG_INF("PMW_DIAG runtime cpi=%u force_awake=%u smart=%u swap_xy=%u invert_x=%u "
                "invert_y=%u report_ms=%u",
                runtime.cpi, runtime.force_awake, runtime.smart_algorithm, runtime.swap_xy,
                runtime.invert_x, runtime.invert_y, runtime.report_interval_min_ms);
    }

    log_register(dev, "product_id", PMW3610_REG_PRODUCT_ID);
    log_register(dev, "revision_id", PMW3610_REG_REVISION_ID);
    log_register(dev, "observation", PMW3610_REG_OBSERVATION);
    log_register(dev, "performance", PMW3610_REG_PERFORMANCE);

    uint8_t motion = 0;
    int16_t motion_x = 0;
    int16_t motion_y = 0;
    int motion_err = read_motion_registers(dev, &motion, &motion_x, &motion_y);
    LOG_INF("PMW_DIAG motion read_err=%d status=0x%02x x=%d y=%d", motion_err, motion,
            motion_x, motion_y);

    if (ready) {
        struct pmw3610_diagnostics diagnostics = {0};
        int err = pmw3610_read_diagnostics(dev, &diagnostics);

        if (err) {
            LOG_ERR("PMW_DIAG surface read_err=%d", err);
        } else {
            LOG_INF("PMW_DIAG surface squal=%u shutter=%u pixel=%u/%u/%u", diagnostics.squal,
                    diagnostics.shutter, diagnostics.pix_min, diagnostics.pix_avg,
                    diagnostics.pix_max);
        }
    }

reschedule:
    k_work_reschedule(k_work_delayable_from_work(work), DIAGNOSTIC_INTERVAL);
}

K_WORK_DELAYABLE_DEFINE(diagnostic_work, diagnostic_work_handler);

static int diagnostic_init(void) {
    k_work_schedule(&diagnostic_work, DIAGNOSTIC_FIRST_DELAY);
    return 0;
}

SYS_INIT(diagnostic_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
