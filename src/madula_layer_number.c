/* Copyright (c) 2026 te9no. SPDX-License-Identifier: MIT */
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk_rgbled_widget/widget.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Only the delayed-work handler owns the pulse state. Event callbacks publish
 * a new desired layer; a change (including key release) replaces the sequence. */
static atomic_t requested_layer = ATOMIC_INIT(0);
static atomic_t restart = ATOMIC_INIT(0);
static unsigned int pulses_left;
static bool lit;

static void pulse_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(pulse_work, pulse_handler);

static void stop_pulses(void) {
    pulses_left = 0;
    lit = false;
    /* Only return a layer-owned LED. Do not clear battery/connectivity status
     * if the widget has preempted the sequence in the meantime. */
    ws2812_clear_status_led(STATUS_LAYER);
}

static void pulse_handler(struct k_work *work) {
    ARG_UNUSED(work);
    const uint16_t lease_ms = CONFIG_RGBLED_WIDGET_LAYER_BLINK_MS * 2 + 100;
    bool first = atomic_cas(&restart, 1, 0);
    if (first) {
        stop_pulses();
        pulses_left = atomic_get(&requested_layer);
        LOG_INF("Madula SPI LED layer %u: %u pulses", pulses_left, pulses_left);
    }
    if (!pulses_left) {
        return;
    }

    uint8_t color = lit ? WS2812_COLOR_BLACK : CONFIG_RGBLED_WIDGET_LAYER_COLOR;
    int err = ws2812_set_status_led(STATUS_LAYER, first ? WS2812_COLOR_BLACK : color,
                                    lease_ms, false);
    if (err < 0) {
        stop_pulses();
        LOG_DBG("Madula layer LED deferred to higher-priority status (%d)", err);
        return;
    }
    if (first) {
        /* The pinned widget keeps an old animation when status changes.
         * Returning our newly acquired lease stops that animation without
         * touching an LED whose higher-priority claim rejected us above. */
        ws2812_clear_status_led(STATUS_LAYER);
        if (ws2812_set_status_led(STATUS_LAYER, color, lease_ms, false) < 0) {
            stop_pulses();
            return;
        }
    }

    lit = !lit;
    if (!lit && --pulses_left == 0) {
        stop_pulses();
        return;
    }
    k_work_reschedule(&pulse_work, K_MSEC(CONFIG_RGBLED_WIDGET_LAYER_BLINK_MS));
}

static int layer_changed(const zmk_event_t *event) {
    ARG_UNUSED(event);
    atomic_val_t layer = zmk_keymap_highest_layer_active();
    if (atomic_set(&requested_layer, layer) != layer) {
        atomic_set(&restart, 1);
        k_work_reschedule(&pulse_work, K_MSEC(CONFIG_RGBLED_WIDGET_LAYER_DEBOUNCE_MS));
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(madula_layer_number, layer_changed);
ZMK_SUBSCRIPTION(madula_layer_number, zmk_layer_state_changed);
