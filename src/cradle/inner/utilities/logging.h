#ifndef CRADLE_INNER_UTILITIES_LOGGING_H
#define CRADLE_INNER_UTILITIES_LOGGING_H

#include <memory>
#include <optional>
#include <string>

#include <cradle/inner/fs/types.h>
#include <spdlog/spdlog.h>

namespace cradle {

void
initialize_logging(
    std::string const& level_spec_arg = "info",
    bool ignore_env_setting = false,
    std::string prefix = "",
    std::optional<file_path> log_file_dir = std::nullopt);

// Throws if a logger with this name already exists
std::shared_ptr<spdlog::logger>
create_logger(std::string const& name);

std::shared_ptr<spdlog::logger>
ensure_logger(std::string const& name);

} // namespace cradle

#endif
