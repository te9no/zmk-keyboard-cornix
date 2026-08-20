/*
 * Copyright (c) 2026 te9no
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <errno.h>
#include <stdbool.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_red), gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_green), gpios);
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_blue), gpios);

static bool initialized;
static struct k_work_delayable turn_off_work;
#if IS_ENABLED(CONFIG_CORNIX_CENTRAL_RGB_LED_BOOT_STATUS)
static struct k_work_delayable boot_status_work;
static uint8_t battery_retry_count;

enum boot_status_stage {
    BOOT_STATUS_BATTERY,
    BOOT_STATUS_INTERVAL,
    BOOT_STATUS_CONNECTION,
    BOOT_STATUS_COMPLETE,
};

static enum boot_status_stage boot_status_stage;
#endif

static void set_rgb(bool red, bool green, bool blue) {
    gpio_pin_set_dt(&red_led, red);
    gpio_pin_set_dt(&green_led, green);
    gpio_pin_set_dt(&blue_led, blue);
}

static void show_active_layer(void) {
    switch (zmk_keymap_highest_layer_active()) {
    case 0:
        set_rgb(false, false, true); // Base: blue
        break;
    case 1:
        set_rgb(true, false, false); // Function: red
        break;
    case 2:
        set_rgb(false, true, false); // Number: green
        break;
    case 3:
        set_rgb(true, true, false); // Adjust: yellow
        break;
    case 4:
        set_rgb(false, true, true); // Navigation: cyan
        break;
    default:
        set_rgb(true, true, true); // Additional layers: white
        break;
    }
}

static void turn_off_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    set_rgb(false, false, false);
}

#if IS_ENABLED(CONFIG_CORNIX_CENTRAL_RGB_LED_BOOT_STATUS)
static void show_battery_status(void) {
#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
    uint8_t level = zmk_battery_state_of_charge();

    if (level == 0) {
        set_rgb(true, true, true); // Battery reading unavailable: white
    } else if (level >= CONFIG_CORNIX_CENTRAL_RGB_LED_BATTERY_LEVEL_HIGH) {
        set_rgb(false, true, false); // High: green
    } else if (level >= CONFIG_CORNIX_CENTRAL_RGB_LED_BATTERY_LEVEL_LOW) {
        set_rgb(true, true, false); // Medium: yellow
    } else {
        set_rgb(true, false, false); // Low: red
    }
#else
    set_rgb(true, true, true); // No battery reporting: white
#endif
}

static void show_connection_status(void) {
    switch (zmk_endpoint_get_selected().transport) {
    case ZMK_TRANSPORT_USB:
        set_rgb(false, true, true); // USB: cyan
        break;
    case ZMK_TRANSPORT_BLE:
        set_rgb(false, false, true); // BLE connected: blue
        break;
    default:
#if IS_ENABLED(CONFIG_ZMK_BLE)
        if (zmk_ble_active_profile_is_open()) {
            set_rgb(true, true, false); // BLE advertising: yellow
            break;
        }
#endif
        set_rgb(true, false, false); // Disconnected: red
        break;
    }
}

static void boot_status_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    switch (boot_status_stage) {
    case BOOT_STATUS_BATTERY:
#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
        if (zmk_battery_state_of_charge() == 0 && battery_retry_count++ < 10) {
            k_work_reschedule(&boot_status_work, K_MSEC(100));
            return;
        }
#endif
        show_battery_status();
        boot_status_stage = BOOT_STATUS_INTERVAL;
        k_work_reschedule(&boot_status_work,
                          K_MSEC(CONFIG_CORNIX_CENTRAL_RGB_LED_STATUS_DURATION_MS));
        break;
    case BOOT_STATUS_INTERVAL:
        set_rgb(false, false, false);
        boot_status_stage = BOOT_STATUS_CONNECTION;
        k_work_reschedule(&boot_status_work,
                          K_MSEC(CONFIG_CORNIX_CENTRAL_RGB_LED_STATUS_INTERVAL_MS));
        break;
    case BOOT_STATUS_CONNECTION:
        show_connection_status();
        boot_status_stage = BOOT_STATUS_COMPLETE;
        k_work_reschedule(&boot_status_work,
                          K_MSEC(CONFIG_CORNIX_CENTRAL_RGB_LED_STATUS_DURATION_MS));
        break;
    case BOOT_STATUS_COMPLETE:
        set_rgb(false, false, false);
        initialized = true;
        LOG_INF("Cornix Central RGB boot status complete");
        break;
    }
}
#endif

static int central_rgb_event_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);

    if (!initialized) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    show_active_layer();
    k_work_reschedule(&turn_off_work,
                      K_MSEC(CONFIG_CORNIX_CENTRAL_RGB_LED_DURATION_MS));

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(cornix_central_rgb, central_rgb_event_listener);
ZMK_SUBSCRIPTION(cornix_central_rgb, zmk_layer_state_changed);

static int central_rgb_init(void) {
    const struct gpio_dt_spec *leds[] = {&red_led, &green_led, &blue_led};

    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        if (!gpio_is_ready_dt(leds[i])) {
            LOG_ERR("Cornix Central RGB GPIO is not ready");
            return -ENODEV;
        }

        int err = gpio_pin_configure_dt(leds[i], GPIO_OUTPUT_INACTIVE);
        if (err < 0) {
            LOG_ERR("Failed to configure Cornix Central RGB GPIO: %d", err);
            return err;
        }
    }

    k_work_init_delayable(&turn_off_work, turn_off_work_handler);
#if IS_ENABLED(CONFIG_CORNIX_CENTRAL_RGB_LED_BOOT_STATUS)
    k_work_init_delayable(&boot_status_work, boot_status_work_handler);
    boot_status_stage = BOOT_STATUS_BATTERY;
    k_work_reschedule(&boot_status_work,
                      K_MSEC(CONFIG_CORNIX_CENTRAL_RGB_LED_BOOT_DELAY_MS));
#else
    initialized = true;
#endif
    LOG_INF("Cornix Central RGB status LED ready");
    return 0;
}

SYS_INIT(central_rgb_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
