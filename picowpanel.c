/**
 * SPDX-FileCopyrightText: 2023 Stephen Merrony
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/watchdog.h"

#include "graphics.h"
#include "info_items.h"
#include "mqtt.h"
#include "rgbmatrix.h"
#include "wifi_config.h"

#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "rgb_led_config.h"

#define RETRY_MS 5000
#define BLINK_PERIOD_MS 1000
#define WATCHDOG_TIMEOUT_MS  3000 // max is ~4700

#define RCV_FROM_IP IP_ADDR_ANY
#define RCV_FROM_PORT 6002

#define DEBUG_printf printf

static image_t * image_ptr;


struct udp_pcb *rcv_udp_pcb;

void on_udp_recv(
    __attribute__((unused)) void *arg,
    __attribute__((unused)) struct udp_pcb *pcb,
    struct pbuf *p,
    __attribute__((unused)) const ip_addr_t *addr,
    __attribute__((unused)) u16_t port) {
    // Receives a chunk of image data and copies it into the image buffer.
    const image_t *img = (const image_t *)(p->payload + 3);
    int poff;
    memcpy(&poff, p->payload, 1);

    memcpy(*image_ptr + poff, *img, 512);

    // Note that the following print limit the throughput to 8fps.
    // printf("DEBUG: poffset: %d\n", poff);
    // printf("DEBUG: img done\n");
    pbuf_free(p);
}

int main() {
    stdio_init_all();

    busy_wait_ms(RETRY_MS); // DEBUGGING - time to connect terminal

    if (watchdog_caused_reboot()) {
        printf("BOOP INFO: Rebooted due to Watchdog timeout\n");
    }

    while (cyw43_arch_init_with_country(WIFI_COUNTRY) != 0) {
        printf("ERROR: WiFi failed to initialise - will retry in 5s\n");
        busy_wait_ms(RETRY_MS);
    }

    cyw43_arch_enable_sta_mode();

    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, WIFI_TIMEOUT_MS) != 0) {
        printf("ERROR: Wifi failed to connect - will retry in 5s\n");
        busy_wait_ms(RETRY_MS);
    }

    /* N.B. The following power management call is critical if you want to achieve smooth
            running w.r.t. MQTT message handling.  Without it, reception appears to stall
            approximately every 3 seconds. */
    int ps = cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));
    printf("DEBUG: cyw43_wifi_pm returned: %d\n", ps);

    printf("DEBUG: Wifi connected\n");

    rcv_udp_pcb = udp_new();

    udp_bind(rcv_udp_pcb, RCV_FROM_IP, RCV_FROM_PORT);
    udp_recv(rcv_udp_pcb, on_udp_recv, NULL);

    image_ptr = core1_setup();

    ii_setup(image_ptr);

    multicore_launch_core1(core1_entry);

    // mqtt_setup_client();
    // mqtt_connect();


    watchdog_enable(WATCHDOG_TIMEOUT_MS, true);

    while (1) {
        busy_wait_ms(BLINK_PERIOD_MS);
        watchdog_update();
    }

    udp_remove(rcv_udp_pcb);
}
