#include <cradle/plugins/requests_storage/http/http_requests_storage.h>
#include <cradle/plugins/requests_storage/http/http_requests_storage_factory.h>

namespace cradle {

std::unique_ptr<secondary_storage_intf>
http_requests_storage_factory::create(
    inner_resources& resources, service_config const& config)
{
    return std::make_unique<http_requests_storage>(resources, config);
}

} // namespace cradle
