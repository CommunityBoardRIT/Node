#pragma once

#include <stdint.h>
#include <arpa/inet.h>

#include "conf/config.h"

#define CONF_SOCKET_BINDING_ADDR_KEY    "CONF_SOCKET_BINDING_ADDR"
#define CONF_SOCKET_BINDING_PORT_KEY    "CONF_SOCKET_BINDING_PORT"

class SocketConfig : public Config
{
private:
    int addr_idx;
    int port_idx;

public:
    SocketConfig();
    ~SocketConfig();

    in_addr_t GetBindingAddress();
    uint16_t GetBindingPort();
};