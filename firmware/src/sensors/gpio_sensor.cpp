#include "gpio_sensor.hpp"
#include "config.hpp"

#include "hardware/gpio.h"
#include "pico/time.h"

#include <cstdio>

namespace sensor
{

    static volatile bool g_event_pending = false;
    static DigitalEvent g_last_event{};

    static uint64_t g_btn_last_us = 0;
    static uint8_t g_btn_last_val = 1;

    static uint64_t g_pir_last_us = 0;
    static uint8_t g_pir_last_val = 0;

    static void irq_callback(uint gpio, uint32_t events)
    {
        uint64_t now = time_us_64();

        if (gpio == config::GPIO_BUTTON_PIN)
        {
            if ((now - g_btn_last_us) < config::DEBOUNCE_TIME_US)
                return;

            uint8_t val = gpio_get(config::GPIO_BUTTON_PIN) ? 1 : 0;
            if (val == g_btn_last_val)
                return;

            g_btn_last_us = now;
            g_btn_last_val = val;
            g_last_event = {.pin = config::GPIO_BUTTON_PIN, .value = val};
            g_event_pending = true;
        }
        else if (gpio == config::GPIO_PIR_PIN)
        {
            if ((now - g_pir_last_us) < config::DEBOUNCE_TIME_US)
                return;

            uint8_t val = gpio_get(config::GPIO_PIR_PIN) ? 1 : 0;
            if (val == g_pir_last_val)
                return;

            g_pir_last_us = now;
            g_pir_last_val = val;
            g_last_event = {.pin = config::GPIO_PIR_PIN, .value = val};
            g_event_pending = true;
        }
    }

    void gpio_init_sensor()
    {
        gpio_init(config::GPIO_BUTTON_PIN);
        gpio_set_dir(config::GPIO_BUTTON_PIN, GPIO_IN);
        gpio_pull_up(config::GPIO_BUTTON_PIN);
        g_btn_last_val = gpio_get(config::GPIO_BUTTON_PIN) ? 1 : 0;

        gpio_init(config::GPIO_PIR_PIN);
        gpio_set_dir(config::GPIO_PIR_PIN, GPIO_IN);
        gpio_pull_down(config::GPIO_PIR_PIN);
        g_pir_last_val = gpio_get(config::GPIO_PIR_PIN) ? 1 : 0;

        gpio_set_irq_enabled_with_callback(
            config::GPIO_BUTTON_PIN,
            GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
            true, &irq_callback);

        gpio_set_irq_enabled(
            config::GPIO_PIR_PIN,
            GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
            true);

        printf("[GPIO] Botao GP%d (valor: %d)\n", config::GPIO_BUTTON_PIN, g_btn_last_val);
        printf("[GPIO] PIR GP%d (valor: %d)\n", config::GPIO_PIR_PIN, g_pir_last_val);
    }

    bool gpio_has_event()
    {
        return g_event_pending;
    }

    DigitalEvent gpio_consume_event()
    {
        g_event_pending = false;
        return g_last_event;
    }

}
