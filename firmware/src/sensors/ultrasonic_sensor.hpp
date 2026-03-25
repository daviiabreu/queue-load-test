#ifndef ULTRASONIC_SENSOR_HPP
#define ULTRASONIC_SENSOR_HPP

#include <cstdint>

namespace sensor
{

    void ultrasonic_init();
    float ultrasonic_read_cm();

}

#endif
