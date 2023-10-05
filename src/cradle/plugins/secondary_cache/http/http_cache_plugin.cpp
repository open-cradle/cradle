#include <cradle/plugins/secondary_cache/http/http_cache.h>
#include <cradle/plugins/secondary_cache/http/http_cache_factory.h>

namespace cradle {

void
activate_http_cache_plugin(inner_resources& resources)
{
    resources.register_secondary_storage_factory(
        http_cache_config_values::PLUGIN_NAME,
        std::make_unique<http_cache_factory>());
}

} // namespace cradle
