#ifndef CRADLE_INNER_REMOTE_LOOPBACK_H
#define CRADLE_INNER_REMOTE_LOOPBACK_H

#include <cradle/inner/service/resources.h>

namespace cradle {

// Configuration keys for the loopback service
struct loopback_config_keys
{
    // (Optional integer)
    // How many asynchronous root requests can run in parallel,
    // on the loopback service
    inline static std::string const ASYNC_CONCURRENCY{
        "loopback/async_concurrency"};
};

/**
 * Registers the loopback service
 *
 * The loopback service simulates a remote executor, but acts locally.
 * It still resolves serialized requests into serialized responses.
 *
 * This will create a new service, overwriting any previous instance
 * (so the new resources will be the active ones).
 */
void
register_loopback_service(
    service_config const& config, inner_resources& resources);

} // namespace cradle

#endif
