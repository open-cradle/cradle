#include <regex>

#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/service/config_map_file.h>
#include <cradle/inner/service/config_map_json.h>
#include <cradle/inner/service/config_map_toml.h>

namespace cradle {

bool
is_toml_file(std::string const& path)
{
    std::regex re("\\.toml$", std::regex_constants::icase);
    return std::regex_search(path, re);
}

service_config_map
read_config_map_from_file(file_path const& path)
{
    std::string path_str{path.string()};
    if (is_toml_file(path_str))
    {
        return read_config_map_from_toml_file(path_str);
    }
    else
    {
        return read_config_map_from_json(read_file_contents(path));
    }
}

} // namespace cradle
