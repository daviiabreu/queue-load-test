#include "adc_sensor.hpp"
#include "config.hpp"
#include "hardware/adc.h"

namespace sensor
{

    static AnalogReading read_channel(uint8_t channel)
    {
        adc_select_input(channel);
        uint16_t raw = adc_read();
        float voltage = static_cast<float>(raw) * config::ADC_VREF / static_cast<float>(config::ADC_MAX_RAW);
        return AnalogReading{.raw_value = raw, .voltage = voltage};
    }

    void adc_init_sensor()
    {
        adc_init();
        adc_gpio_init(config::ADC_POT_GPIO);
        adc_gpio_init(config::ADC_LDR_GPIO);
    }

    AnalogReading adc_read_pot()
    {
        return read_channel(config::ADC_POT_CHANNEL);
    }

    AnalogReading adc_read_ldr()
    {
        return read_channel(config::ADC_LDR_CHANNEL);
    }

}
