#include "wifi_manager.hpp"
#include "config.hpp"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include <cstdio>

namespace wifi
{

    static State s_state = State::DISCONNECTED;
    static uint32_t s_last_reconnect_ms = 0;

    static uint32_t auth_mode()
    {
        return (config::WIFI_PASSWORD[0] != '\0') ? CYW43_AUTH_WPA2_AES_PSK : CYW43_AUTH_OPEN;
    }

    State state()
    {
        return s_state;
    }

    bool is_connected()
    {
        return cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_JOIN;
    }

    bool init_and_connect()
    {
        if (cyw43_arch_init())
        {
            printf("[WIFI] ERRO: falha ao inicializar CYW43\n");
            return false;
        }

        cyw43_arch_enable_sta_mode();
        printf("[WIFI] Conectando a \"%s\"...\n", config::WIFI_SSID);

        for (int i = 0; i < 3; i++)
        {
            s_state = State::CONNECTING;

            int err = cyw43_arch_wifi_connect_timeout_ms(
                config::WIFI_SSID,
                config::WIFI_PASSWORD,
                auth_mode(),
                15000);

            if (err == 0)
            {
                s_state = State::CONNECTED;
                printf("[WIFI] Conectado!\n");
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                return true;
            }

            s_state = State::DISCONNECTED;
            printf("[WIFI] Falha (tentativa %d/3, erro=%d)\n", i + 1, err);
            sleep_ms(2000);
        }

        printf("[WIFI] Nao foi possivel conectar\n");
        return false;
    }

    void poll()
    {
        cyw43_arch_poll();

        switch (s_state)
        {
        case State::CONNECTED:
            if (!is_connected())
            {
                printf("[WIFI] Conexao perdida\n");
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                s_state = State::DISCONNECTED;
            }
            break;

        case State::DISCONNECTED:
        {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (now - s_last_reconnect_ms >= config::WIFI_RECONNECT_COOLDOWN_MS)
            {
                s_last_reconnect_ms = now;
                s_state = State::CONNECTING;
                printf("[WIFI] Reconectando...\n");

                int err = cyw43_arch_wifi_connect_timeout_ms(
                    config::WIFI_SSID,
                    config::WIFI_PASSWORD,
                    auth_mode(),
                    15000);

                if (err == 0)
                {
                    s_state = State::CONNECTED;
                    printf("[WIFI] Reconectado!\n");
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                }
                else
                {
                    s_state = State::DISCONNECTED;
                    printf("[WIFI] Falha na reconexao (erro=%d)\n", err);
                }
            }
            break;
        }

        case State::CONNECTING:
            break;
        }
    }

}
