#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <cstdint>

namespace http
{

    bool post(const char *json_body, uint16_t body_len);

}

#endif
