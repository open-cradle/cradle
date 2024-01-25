#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_HTTP_HTTP_CACHE_IMPL_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_HTTP_HTTP_CACHE_IMPL_H

#include <memory>
#include <optional>
#include <string>

#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/io/http_requests.h>

namespace cradle {

class inner_resources;

class http_cache_impl
{
 public:
    http_cache_impl(inner_resources& resources);

    // Returns std::nullopt if the value is not in the cache.
    // Throws on other errors.
    cppcoro::task<std::optional<blob>>
    read(std::string key);

    cppcoro::task<void>
    write(std::string key, blob value);

 private:
    inner_resources& resources_;
    int port_;
    std::shared_ptr<spdlog::logger> logger_;

    cppcoro::task<std::optional<std::string>>
    get_string_via_http(http_request query);

    cppcoro::task<std::optional<blob>>
    get_blob_via_http(http_request query);

    cppcoro::task<void>
    put_via_http(http_request query);
};

} // namespace cradle

#endif
