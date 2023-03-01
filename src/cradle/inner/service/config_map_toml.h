#ifndef CRADLE_INNER_SERVICE_CONFIG_MAP_TOML_H
#define CRADLE_INNER_SERVICE_CONFIG_MAP_TOML_H

#include <string>

#include <cradle/inner/service/config.h>

namespace cradle {

// Reads a configuration map from a TOML string.
// Errors lead to an exception being thrown.
service_config_map
read_config_map_from_toml(std::string const& toml_text);

// Reads a configuration map from a TOML file.
// Errors lead to an exception being thrown.
service_config_map
read_config_map_from_toml_file(std::string const& path);

} // namespace cradle

#endif
