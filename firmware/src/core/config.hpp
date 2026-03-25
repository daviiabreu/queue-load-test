#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>

namespace config
{

    constexpr uint8_t GPIO_BUTTON_PIN = 16;
    constexpr uint8_t GPIO_PIR_PIN = 17;
    constexpr uint8_t HC_SR04_TRIG_PIN = 18;
    constexpr uint8_t HC_SR04_ECHO_PIN = 19;
    constexpr uint8_t ADC_POT_GPIO = 26;
    constexpr uint8_t ADC_POT_CHANNEL = 0;
    constexpr uint8_t ADC_LDR_GPIO = 27;
    constexpr uint8_t ADC_LDR_CHANNEL = 1;

    constexpr const char *WIFI_SSID = "Wokwi-GUEST";
    constexpr const char *WIFI_PASSWORD = "";

    constexpr const char *BACKEND_HOST = "host.wokwi.internal";
    constexpr uint16_t BACKEND_PORT = 8080;
    constexpr const char *BACKEND_PATH = "/api/telemetry";

    constexpr const char *DEVICE_ID = "pico-w-001";

    constexpr uint32_t ADC_SAMPLE_INTERVAL_MS = 1000;
    constexpr uint64_t DEBOUNCE_TIME_US = 50000;

    constexpr float ADC_VREF = 3.3f;
    constexpr uint16_t ADC_MAX_RAW = 4095;

    constexpr uint64_t HC_SR04_ECHO_WAIT_TIMEOUT_US = 30000;
    constexpr uint64_t HC_SR04_ECHO_MEASURE_TIMEOUT_US = 38000;
    constexpr float SOUND_US_PER_CM = 58.0f;
    constexpr uint32_t WIFI_RECONNECT_COOLDOWN_MS = 10000;

}

#endif
