#ifndef CRADLE_RPCLIB_CLIENT_CONTAINED_PROXY_POOL_H
#define CRADLE_RPCLIB_CLIENT_CONTAINED_PROXY_POOL_H

#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include <spdlog/spdlog.h>

#include <cradle/rpclib/client/ephemeral_port_owner.h>

namespace cradle {

class rpclib_client;
class service_config;

// A pool of rpclib_client objects communicating to the contained processes
// (rpclib server instances running in contained mode).
class contained_proxy_pool
{
 public:
    // Allocates an rpclib_client object from the pool.
    //
    // Uses the DEPLOY_DIR config item (if set).
    std::unique_ptr<rpclib_client>
    alloc_proxy(
        service_config const& config, std::shared_ptr<spdlog::logger> logger);

    // Returns an rpclib_client object to the pool.
    // Should be called after the process finished running its function.
    //
    // succeeded should be true if the function succeeded; if so, the process
    // may be kept alive for reuse. If anything went wrong, the process is
    // deemed unreliable and must be killed.
    void
    free_proxy(std::unique_ptr<rpclib_client> proxy, bool succeeded);

 private:
    std::mutex mutex_;
    ephemeral_port_owner port_owner_;
    std::deque<std::unique_ptr<rpclib_client>> available_proxies_;
};

} // namespace cradle

#endif
