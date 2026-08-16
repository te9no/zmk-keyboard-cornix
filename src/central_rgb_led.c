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
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_red), gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_green), gpios);
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(led_blue), gpios);

static bool initialized;
static struct k_work_delayable turn_off_work;

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
    initialized = true;
    LOG_INF("Cornix Central RGB layer blink ready");
    return 0;
}

SYS_INIT(central_rgb_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
