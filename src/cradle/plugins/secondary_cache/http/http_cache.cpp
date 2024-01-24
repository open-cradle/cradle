#include <fmt/format.h>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/io/http_requests.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/plugins/secondary_cache/http/http_cache.h>
#include <cradle/plugins/secondary_cache/http/http_cache_impl.h>

namespace cradle {

/*
 * The implementation expects a bazel-remote server.
 *
 * Like the memory (immutable) and local disk caches, the HTTP cache implements
 * a two-phase solution, using two subcaches, called the Action Cache (AC) and
 * the Content Addressable Storage (CAS), respectively. bazel-remote already
 * implements these, via the /ac/<key> and /cas/<key> endpoints.
 *
 * An Action corresponds to resolving a request. Records in the Action Cache
 * are indexed by SHA-2 strings that uniquely identify a request. bazel-remote
 * normally expects the Action Cache to contain ActionResult's; however, in a
 * CRADLE context, converting digests to and from ActionResult's would be a
 * major complication without any advantages. The easiest solution is to store
 * digests under /ac/<key>, and instruct the server not to check that blob
 * contents encode an ActionResult, by passing --disable_http_ac_validation.
 *
 * The CAS stores the result values, indexed by unique digests over those
 * values. Thus, if two different requests result in the same value, the
 * corresponding AC records will reference the same CAS record.
 * A CAS key is the lowercase SHA256 hash of the stored value. This is
 * identical between CRADLE and Bazel.
 * A CAS value is a blob that serializes the actual value. Serialization
 * details are up to the HTTP cache client.
 */

namespace {

std::string
make_url(int port, std::string const& cache_name, std::string const& key)
{
    return fmt::format("http://localhost:{}/{}/{}", port, cache_name, key);
}

http_request
make_cache_get_request(
    int port, std::string const& cache_name, std::string const& key)
{
    return make_get_request(
        make_url(port, cache_name, key), {{"Accept", "*/*"}});
}

http_request
make_ac_get_request(int port, std::string const& ac_key)
{
    return make_cache_get_request(port, "ac", ac_key);
}

http_request
make_cas_get_request(int port, std::string const& digest)
{
    return make_cache_get_request(port, "cas", digest);
}

http_request
make_cache_put_request(
    int port,
    std::string const& cache_name,
    std::string const& key,
    blob value)
{
    return http_request{
        http_request_method::PUT,
        make_url(port, cache_name, key),
        {{"Accept", "*/*"}},
        std::move(value),
        none};
}

http_request
make_ac_put_request(
    int port, std::string const& key, std::string const& digest)
{
    return make_cache_put_request(port, "ac", key, make_blob(digest));
}

http_request
make_cas_put_request(int port, std::string const& key, blob value)
{
    return make_cache_put_request(port, "cas", key, value);
}

} // namespace

http_cache_impl::http_cache_impl(inner_resources& resources)
    : resources_{resources},
      port_{static_cast<int>(resources.config().get_mandatory_number(
          http_cache_config_keys::PORT))},
      logger_{ensure_logger("http_cache")}
{
}

cppcoro::task<std::optional<blob>>
http_cache_impl::read(std::string key)
{
    logger_->info("read {}", key);
    auto opt_digest
        = co_await get_string_via_http(make_ac_get_request(port_, key));
    if (!opt_digest)
    {
        co_return std::nullopt;
    }
    co_return co_await get_blob_via_http(
        make_cas_get_request(port_, *opt_digest));
}

cppcoro::task<std::optional<std::string>>
http_cache_impl::get_string_via_http(http_request query)
{
    auto opt_blob = co_await get_blob_via_http(std::move(query));
    if (!opt_blob)
    {
        co_return std::nullopt;
    }
    co_return to_string(*opt_blob);
}

cppcoro::task<std::optional<blob>>
http_cache_impl::get_blob_via_http(http_request query)
{
    logger_->info("  GET {}", query.url);
    http_response response;
    try
    {
        // Throws if status code is not 2xx
        response = co_await resources_.async_http_request(std::move(query));
    }
    catch (bad_http_status_code& e)
    {
        // 404 means the value is not in the cache.
        // Anything else is treated as an error.
        auto status_code{
            get_required_error_info<http_response_info>(e).status_code};
        if (status_code != 404)
        {
            logger_->error("    GET failed with status code {}", status_code);
            throw;
        }
        logger_->info("    not found (404)");
        co_return std::nullopt;
    }
    logger_->info("    OK");
    co_return response.body;
}

cppcoro::task<void>
http_cache_impl::write(std::string key, blob value)
{
    logger_->info("write {}", key);
    auto digest{get_unique_string_tmpl(value)};

    // Put the value in the CAS
    co_await put_via_http(
        make_cas_put_request(port_, digest, std::move(value)));

    // Put the digest in the AC
    co_await put_via_http(make_ac_put_request(port_, key, std::move(digest)));

    co_return;
}

cppcoro::task<void>
http_cache_impl::put_via_http(http_request query)
{
    logger_->info("  PUT {}", query.url);
    try
    {
        // Throws if status_code is not 2xx
        co_await resources_.async_http_request(std::move(query));
    }
    catch (bad_http_status_code& e)
    {
        auto status_code{
            get_required_error_info<http_response_info>(e).status_code};
        logger_->error("    PUT failed with status code {}", status_code);
        throw;
    }
    logger_->info("    OK");
}

http_cache::http_cache(inner_resources& resources)
    : impl_{std::make_unique<http_cache_impl>(resources)}
{
}

http_cache::~http_cache() = default;

void
http_cache::clear()
{
    throw not_implemented_error();
}

cppcoro::task<std::optional<blob>>
http_cache::read(std::string key)
{
    return impl_->read(std::move(key));
}

cppcoro::task<void>
http_cache::write(std::string key, blob value)
{
    return impl_->write(std::move(key), std::move(value));
}

} // namespace cradle
