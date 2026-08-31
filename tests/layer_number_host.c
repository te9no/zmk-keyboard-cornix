#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef int atomic_t;
typedef int atomic_val_t;
typedef int zmk_event_t;
struct k_work { int unused; };
struct k_work_delayable { struct k_work work; };
#define ATOMIC_INIT(x) (x)
#define ARG_UNUSED(x) ((void)(x))
#define K_MSEC(x) (x)
#define LOG_MODULE_DECLARE(...)
#define LOG_INF(...)
#define LOG_DBG(...)
#define ZMK_LISTENER(...)
#define ZMK_SUBSCRIPTION(...)
#define ZMK_EV_EVENT_BUBBLE 0
#define K_WORK_DELAYABLE_DEFINE(name, handler) static struct k_work_delayable name
#define CONFIG_RGBLED_WIDGET_LAYER_BLINK_MS 120
#define CONFIG_RGBLED_WIDGET_LAYER_DEBOUNCE_MS 100
#define CONFIG_RGBLED_WIDGET_LAYER_COLOR 7
#define WS2812_COLOR_BLACK 0
#define STATUS_LAYER 2

static int active_layer, scheduled, ons, color;
static bool blocked;
static int atomic_get(atomic_t *a) { return *a; }
static int atomic_set(atomic_t *a, int n) { int old = *a; *a = n; return old; }
static bool atomic_cas(atomic_t *a, int old, int n) {
    if (*a != old) return false;
    *a = n;
    return true;
}
static int zmk_keymap_highest_layer_active(void) { return active_layer; }
static int k_work_reschedule(struct k_work_delayable *w, int ms) {
    (void)w;
    assert(ms == 100 || ms == 120);
    scheduled = 1;
    return 0;
}
static int ws2812_clear_status_led(int status) {
    assert(status == STATUS_LAYER);
    if (!blocked) color = 0;
    return 0;
}
static int ws2812_set_status_led(int status, uint8_t next, uint16_t lease, bool persistent) {
    assert(status == STATUS_LAYER && lease == 340 && !persistent);
    if (blocked) return -16;
    if (!color && next) ons++;
    color = next;
    return 0;
}

#include "../src/madula_layer_number.c"

static void reset(void) {
    requested_layer = restart = active_layer = scheduled = ons = color = 0;
    pulses_left = 0;
    lit = blocked = false;
}
static void change(int layer) { active_layer = layer; layer_changed(NULL); }
static void tick(void) { scheduled = 0; pulse_handler(NULL); }
static void finish(void) {
    for (int n = 0; scheduled && n < 100; n++) tick();
    assert(!scheduled && !color && !pulses_left);
}
int main(void) {
    for (int layer = 0; layer <= 31; layer++) {
        reset(); change(layer); finish(); assert(ons == layer);
    }
    /* Key release cancels the pending number instead of finishing stale work. */
    reset(); change(5); tick(); change(0); finish(); assert(ons == 1);
    reset(); change(5); tick(); change(2); finish(); assert(ons == 3);
    /* An event that does not change the highest layer must not restart it. */
    reset(); change(2); tick(); change(2); finish(); assert(ons == 2);
    /* Higher-priority widget status is never overwritten, including mid-pulse. */
    reset(); blocked = true; color = 6; change(3); tick();
    assert(!scheduled && ons == 0 && color == 6);
    reset(); change(3); tick(); blocked = true; color = 6; tick();
    assert(!scheduled && ons == 1 && color == 6);
    return 0;
}
