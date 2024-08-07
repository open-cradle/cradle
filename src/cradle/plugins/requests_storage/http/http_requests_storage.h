#ifndef CRADLE_PLUGINS_REQUESTS_STORAGE_HTTP_HTTP_REQUESTS_STORAGE_H
#define CRADLE_PLUGINS_REQUESTS_STORAGE_HTTP_HTTP_REQUESTS_STORAGE_H

#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/secondary_storage_intf.h>

namespace cradle {

// Implements a remote requests storage via HTTP requests to a local server.
// Assumptions:
// - The server is reachable as http://localhost.
// - The server is already running.
// - Keys are SHA256 values.

// Configuration keys for the HTTP storage plugin
struct http_requests_storage_config_keys
{
    // (Mandatory integer)
    // HTTP port
    inline static std::string const PORT{"http_requests_storage/port"};
};

class http_requests_storage : public secondary_storage_intf
{
 public:
    http_requests_storage(inner_resources& resources);

    std::string const&
    name() const override
    {
        return name_;
    }

    // Not (yet?) implemented
    void
    clear() override;

    // Returns std::nullopt if the value is not in the storage.
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
    std::string const name_{"http_requests"};
    inner_resources& resources_;
    int port_;
};

} // namespace cradle

#endif
