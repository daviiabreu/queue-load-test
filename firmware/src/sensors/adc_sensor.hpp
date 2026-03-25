#ifndef ADC_SENSOR_HPP
#define ADC_SENSOR_HPP

#include <cstdint>

namespace sensor
{

    struct AnalogReading
    {
        uint16_t raw_value;
        float voltage;
    };

    void adc_init_sensor();
    AnalogReading adc_read_pot();
    AnalogReading adc_read_ldr();

}

#endif
