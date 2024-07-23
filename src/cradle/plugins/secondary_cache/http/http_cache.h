#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_HTTP_HTTP_CACHE_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_HTTP_HTTP_CACHE_H

#include <cradle/inner/service/secondary_storage_intf.h>

namespace cradle {

class http_cache_impl;
class inner_resources;

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

class http_cache : public secondary_storage_intf
{
 public:
    http_cache(inner_resources& resources);

    ~http_cache();

    // Not (yet?) implemented
    void
    clear() override;

    // Returns std::nullopt if the value is not in the cache.
    // Throws on other errors.
    cppcoro::task<std::optional<blob>>
    read(std::string key) override;

    cppcoro::task<void>
    write(std::string key, blob value) override;

    bool
    allow_blob_files() const override
    {
        return false;
    }

 private:
    std::unique_ptr<http_cache_impl> impl_;
};

} // namespace cradle

#endif
