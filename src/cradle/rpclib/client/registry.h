#ifndef CRADLE_RPCLIB_CLIENT_REGISTRY_H
#define CRADLE_RPCLIB_CLIENT_REGISTRY_H

#include <memory>

#include <cradle/inner/service/config.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

std::shared_ptr<rpclib_client>
register_rpclib_client(service_config const& config);

} // namespace cradle

#endif
