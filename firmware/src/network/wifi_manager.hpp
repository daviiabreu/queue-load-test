#ifndef WIFI_MANAGER_HPP
#define WIFI_MANAGER_HPP

namespace wifi
{

    enum class State { DISCONNECTED, CONNECTING, CONNECTED };

    bool init_and_connect();
    bool is_connected();
    State state();
    void poll();

}

#endif
