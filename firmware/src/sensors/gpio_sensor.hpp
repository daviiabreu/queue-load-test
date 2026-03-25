#ifndef GPIO_SENSOR_HPP
#define GPIO_SENSOR_HPP

#include <cstdint>

namespace sensor
{

    struct DigitalEvent
    {
        uint8_t pin;
        uint8_t value;
    };

    void gpio_init_sensor();
    bool gpio_has_event();
    DigitalEvent gpio_consume_event();

}

#endif
