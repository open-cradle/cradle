#ifndef CRADLE_RPCLIB_CLI_CMD_COMMON_H
#define CRADLE_RPCLIB_CLI_CMD_COMMON_H

#include <memory>

#include <spdlog/spdlog.h>

#include <cradle/inner/service/config.h>

namespace cradle {

struct cli_options;

service_config_map
create_config_map(cli_options const& options);

std::shared_ptr<spdlog::logger>
create_logger(cli_options const& options);

} // namespace cradle

#endif
