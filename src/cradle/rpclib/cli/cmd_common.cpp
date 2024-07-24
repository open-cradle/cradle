#include <cradle/inner/utilities/logging.h>
#include <cradle/rpclib/cli/cmd_common.h>
#include <cradle/rpclib/cli/types.h>
#include <cradle/rpclib/common/config.h>

namespace cradle {

service_config_map
create_config_map(cli_options const& options)
{
    service_config_map config_map;
    config_map[rpclib_config_keys::EXPECT_SERVER] = true;
    config_map[rpclib_config_keys::PORT_NUMBER] = options.port;
    return config_map;
}

std::shared_ptr<spdlog::logger>
create_logger(cli_options const& options)
{
    initialize_logging(options.log_level, options.log_level_set, "cli ");
    return create_logger("cli");
}

} // namespace cradle
