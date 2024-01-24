#include <fmt/format.h>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/io/http_requests.h>
#include <cradle/plugins/requests_storage/http/http_requests_storage.h>

namespace cradle {

namespace {

/*
 * The implementation expects a bazel-remote server.
 *
 * Entries would preferably be stored under /cas/<key>, but the server checks
 * that key equals the SHA256 over the blob. In CRADLE context, this is not
 * so: the key is the SHA256 over the request whose result is the blob.
 * The easiest solution is to store entries under /ac/<key>, and instruct
 * the server not to check that blob contents encode an ActionResult, by
 * passing --disable_http_ac_validation=1.
 * An alternative would be to simulate Bazel, and store an ActionResult
 * referring to the blob in the CAS.
 * Different requests will never serialize to the same value, so a two-phase
 * approach would not be useful here.
 */

std::string
make_url(int port, std::string const& key)
{
    return fmt::format("http://localhost:{}/ac/{}", port, key);
}

http_request
make_http_get_request(int port, std::string const& key)
{
    return make_get_request(make_url(port, key), {{"Accept", "*/*"}});
}

http_request
make_http_put_request(int port, std::string const& key, blob value)
{
    return http_request{
        http_request_method::PUT,
        make_url(port, key),
        {{"Accept", "*/*"}},
        std::move(value),
        none};
}

} // namespace

http_requests_storage::http_requests_storage(inner_resources& resources)
    : resources_{resources},
      port_{static_cast<int>(resources.config().get_mandatory_number(
          http_requests_storage_config_keys::PORT))}
{
}

void
http_requests_storage::clear()
{
    throw not_implemented_error();
}

cppcoro::task<std::optional<blob>>
http_requests_storage::read(std::string key)
{
    auto query = make_http_get_request(port_, key);
    http_response response;
    try
    {
        // Throws if status code is not 2xx
        response = co_await resources_.async_http_request(std::move(query));
    }
    catch (bad_http_status_code& e)
    {
        // 404 means the value is not in the storage.
        // Anything else is treated as an error.
        if (get_required_error_info<http_response_info>(e).status_code != 404)
        {
            throw;
        }
        co_return std::nullopt;
    }
    co_return std::move(response.body);
}

cppcoro::task<void>
http_requests_storage::write(std::string key, blob value)
{
    auto query = make_http_put_request(port_, key, std::move(value));
    // Throws if status_code is not 2xx
    co_await resources_.async_http_request(std::move(query));
    co_return;
}

} // namespace cradle
