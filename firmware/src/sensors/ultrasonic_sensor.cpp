#include "ultrasonic_sensor.hpp"
#include "config.hpp"

#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include <cstdio>

namespace sensor
{

    void ultrasonic_init()
    {
        gpio_init(config::HC_SR04_TRIG_PIN);
        gpio_set_dir(config::HC_SR04_TRIG_PIN, GPIO_OUT);
        gpio_put(config::HC_SR04_TRIG_PIN, 0);

        gpio_init(config::HC_SR04_ECHO_PIN);
        gpio_set_dir(config::HC_SR04_ECHO_PIN, GPIO_IN);

        printf("[ULTRASONIC] Inicializado TRIG=GP%d ECHO=GP%d\n",
               config::HC_SR04_TRIG_PIN, config::HC_SR04_ECHO_PIN);
    }

    float ultrasonic_read_cm()
    {
        gpio_put(config::HC_SR04_TRIG_PIN, 1);
        sleep_us(10);
        gpio_put(config::HC_SR04_TRIG_PIN, 0);

        uint64_t wait_deadline = time_us_64() + config::HC_SR04_ECHO_WAIT_TIMEOUT_US;
        while (!gpio_get(config::HC_SR04_ECHO_PIN))
        {
            cyw43_arch_poll();
            if (time_us_64() > wait_deadline)
                return -1.0f;
        }

        uint64_t start = time_us_64();
        uint64_t measure_deadline = start + config::HC_SR04_ECHO_MEASURE_TIMEOUT_US;
        while (gpio_get(config::HC_SR04_ECHO_PIN))
        {
            cyw43_arch_poll();
            if (time_us_64() > measure_deadline)
                return -1.0f;
        }

        return static_cast<float>(time_us_64() - start) / config::SOUND_US_PER_CM;
    }

}
