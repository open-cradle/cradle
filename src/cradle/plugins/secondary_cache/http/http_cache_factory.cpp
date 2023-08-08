#include <cradle/inner/service/resources.h>
#include <cradle/plugins/secondary_cache/http/http_cache.h>
#include <cradle/plugins/secondary_cache/http/http_cache_factory.h>

namespace cradle {

std::unique_ptr<secondary_storage_intf>
http_cache_factory::create(
    inner_resources& resources, service_config const& config)
{
    return std::make_unique<http_cache>(resources, config);
}

} // namespace cradle
