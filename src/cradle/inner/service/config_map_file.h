#ifndef CRADLE_INNER_SERVICE_CONFIG_MAP_FILE_H
#define CRADLE_INNER_SERVICE_CONFIG_MAP_FILE_H

#include <string>

#include <cradle/inner/fs/types.h>
#include <cradle/inner/service/config.h>

namespace cradle {

bool
is_toml_file(std::string const& path);

// Reads a configuration map from a file.
// Errors lead to an exception being thrown.
service_config_map
read_config_map_from_file(file_path const& path);

} // namespace cradle

#endif
