#ifndef CRADLE_RPCLIB_CLIENT_REGISTRY_H
#define CRADLE_RPCLIB_CLIENT_REGISTRY_H

#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

rpclib_client&
register_rpclib_client(
    service_config const& config, inner_resources& resources);

} // namespace cradle

#endif
