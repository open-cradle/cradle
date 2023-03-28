#ifndef CRADLE_INNER_UTILITIES_LOGGING_H
#define CRADLE_INNER_UTILITIES_LOGGING_H

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace cradle {

void
initialize_logging(
    std::string const& level_spec_arg = "info",
    bool ignore_env_setting = false);

std::shared_ptr<spdlog::logger>
create_logger(std::string const& name);

} // namespace cradle

#endif
