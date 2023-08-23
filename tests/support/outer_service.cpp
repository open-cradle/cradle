#include "outer_service.h"
#include <cradle/deploy_dir.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache.h>
#include <cradle/plugins/secondary_cache/local/local_disk_cache_plugin.h>
#include <cradle/thinknode/service/core.h>

namespace cradle {

static std::string tests_cache_dir{"tests_cache"};
static service_config_map const outer_config_map{
    {generic_config_keys::TESTING, true},
    {generic_config_keys::DEPLOY_DIR, get_deploy_dir()},
    {inner_config_keys::MEMORY_CACHE_UNUSED_SIZE_LIMIT, 0x40'00'00'00U},
    {inner_config_keys::SECONDARY_CACHE_FACTORY,
     local_disk_cache_config_values::PLUGIN_NAME},
    {local_disk_cache_config_keys::DIRECTORY, tests_cache_dir},
    {local_disk_cache_config_keys::SIZE_LIMIT, 0x40'00'00'00U},
    {inner_config_keys::HTTP_CONCURRENCY, 2U}};

service_config
make_outer_tests_config()
{
    return service_config{outer_config_map};
}

void
init_test_service(service_core& core)
{
    activate_local_disk_cache_plugin();
    reset_directory(file_path(tests_cache_dir));
    core.initialize(make_outer_tests_config());
}

} // namespace cradle
