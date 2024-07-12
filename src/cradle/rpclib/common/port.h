#ifndef CRADLE_RPCLIB_COMMON_PORT_H
#define CRADLE_RPCLIB_COMMON_PORT_H

namespace cradle {

using rpclib_port_t = uint16_t;

static const inline rpclib_port_t RPCLIB_PORT_PRODUCTION = 8098;
static const inline rpclib_port_t RPCLIB_PORT_TESTING = 8096;

} // namespace cradle

#endif
