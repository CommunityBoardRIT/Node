#include <stdint.h>
#include <arpa/inet.h>

#include "socket/socket_conf.h"

SocketConfig::SocketConfig ()
{
    addr_idx = AddKey(CONF_SOCKET_BINDING_ADDR_KEY, 0);
    port_idx = AddKey(CONF_SOCKET_BINDING_PORT_KEY, 1208);
}

SocketConfig::~SocketConfig()
{

}

/**
 * @details Returns in network-byte order; NOT HOST ORDER
 */
in_addr_t SocketConfig::GetBindingAddress()
{
    const char* val = GetVal(addr_idx);
    return inet_addr(val);
}

uint16_t SocketConfig::GetBindingPort()
{
    return (uint16_t) GetInt(port_idx);
}