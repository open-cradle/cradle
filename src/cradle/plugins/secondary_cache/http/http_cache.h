#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_HTTP_HTTP_CACHE_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_HTTP_HTTP_CACHE_H

#include <cradle/inner/caching/secondary_cache_intf.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

// Implements a remote cache via HTTP requests to a local server.
// Assumptions:
// - The server is reachable as http://localhost.
// - The server is already running.
// - Keys are SHA256 values.

// Configuration keys for the HTTP storage plugin
struct http_cache_config_keys
{
    // (Mandatory integer)
    // HTTP port
    inline static std::string const PORT{"http_cache/port"};
};

struct http_cache_config_values
{
    // Value for the inner_config_keys::SECONDARY_CACHE_FACTORY config
    inline static std::string const PLUGIN_NAME{"http_cache"};
};

class http_cache : public secondary_cache_intf
{
 public:
    http_cache(inner_resources& resources, service_config const& config);

    // Not (yet?) implemented
    void
    reset(service_config const& config) override;

    // Returns blob{} if the value is not in the cache.
    // Throws on other errors.
    cppcoro::task<blob>
    read(std::string key) override;

    cppcoro::task<void>
    write(std::string key, blob value) override;

 private:
    inner_resources& resources_;
    int port_;
};

} // namespace cradle

#endif
