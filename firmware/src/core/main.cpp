#include "config.hpp"
#include "adc_sensor.hpp"
#include "gpio_sensor.hpp"
#include "ultrasonic_sensor.hpp"
#include "wifi_manager.hpp"
#include "http_client.hpp"

#include "pico/stdlib.h"
#include "pico/time.h"
#include <cstdio>

static volatile bool g_sample_pending = false;

static bool on_sample_timer(repeating_timer_t *rt)
{
    (void)rt;
    g_sample_pending = true;
    return true;
}

static void send(const char *sensor_type, const char *reading_type, float value)
{
    uint32_t secs = to_ms_since_boot(get_absolute_time()) / 1000;

    char json[256];
    int len = snprintf(json, sizeof(json),
                       "{\"device_id\":\"%s\","
                       "\"timestamp\":\"2026-03-23T%02u:%02u:%02uZ\","
                       "\"sensor_type\":\"%s\","
                       "\"reading_type\":\"%s\","
                       "\"value\":%.2f}",
                       config::DEVICE_ID,
                       (secs / 3600) % 24, (secs / 60) % 60, secs % 60,
                       sensor_type, reading_type,
                       static_cast<double>(value));

    if (len > 0)
    {
        printf("[MAIN] Enviando: %s\n", json);
        http::post(json, static_cast<uint16_t>(len));
    }
}

int main()
{
    stdio_init_all();
    sleep_ms(2000);

    printf("=== Pico W Telemetry ===\n");
    printf("Device: %s\n", config::DEVICE_ID);
    printf("Backend: %s:%u%s\n",
           config::BACKEND_HOST, config::BACKEND_PORT, config::BACKEND_PATH);

    sensor::adc_init_sensor();
    sensor::gpio_init_sensor();
    sensor::ultrasonic_init();

    if (!wifi::init_and_connect())
    {
        printf("[MAIN] Sem WiFi, abortando\n");
        while (true)
            sleep_ms(1000);
    }

    printf("[MAIN] Pronto!\n\n");

    repeating_timer_t sample_timer;
    add_repeating_timer_ms(config::ADC_SAMPLE_INTERVAL_MS, on_sample_timer, nullptr, &sample_timer);

    while (true)
    {
        wifi::poll();

        if (sensor::gpio_has_event())
        {
            auto event = sensor::gpio_consume_event();
            if (event.pin == config::GPIO_BUTTON_PIN)
            {
                printf("[MAIN] Botao: %u\n", event.value);
                send("presence", "discrete", static_cast<float>(event.value));
            }
            else if (event.pin == config::GPIO_PIR_PIN)
            {
                printf("[MAIN] PIR: %u\n", event.value);
                send("motion", "discrete", static_cast<float>(event.value));
            }
        }

        if (g_sample_pending)
        {
            g_sample_pending = false;

            auto pot = sensor::adc_read_pot();
            printf("[MAIN] POT: raw=%u, %.2f V\n",
                   pot.raw_value, static_cast<double>(pot.voltage));
            send("temperature", "analog", pot.voltage);

            auto ldr = sensor::adc_read_ldr();
            printf("[MAIN] LDR: raw=%u, %.2f V\n",
                   ldr.raw_value, static_cast<double>(ldr.voltage));
            send("luminosity", "analog", ldr.voltage);

            float distance = sensor::ultrasonic_read_cm();
            if (distance >= 0)
            {
                printf("[MAIN] HC-SR04: %.1f cm\n", static_cast<double>(distance));
                send("distance", "analog", distance);
            }
        }

        sleep_ms(10);
    }

    return 0;
}
