#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <cradle/inner/service/config_map_to_json.h>

namespace cradle {

// Like Python's key.split('/')
auto
split_key(std::string const& key)
{
    std::vector<std::string> res;
    std::string::size_type start = 0;
    std::string::size_type end;
    while ((end = key.find("/", start)) != std::string::npos)
    {
        res.push_back(key.substr(start, end - start));
        start = end + 1;
    }
    res.push_back(key.substr(start));
    return res;
}

std::string
write_config_map_to_json(service_config_map const& map)
{
    nlohmann::json j;
    for (auto const& [key, value] : map)
    {
        auto key_vector{split_key(key)};
        if (key_vector.size() > 2)
        {
            throw std::invalid_argument{fmt::format("Invalid key {}", key)};
        }
        std::visit(
            [&](auto&& arg) {
                if (key_vector.size() == 1)
                {
                    j[key_vector[0]] = arg;
                }
                else
                {
                    j[key_vector[0]][key_vector[1]] = arg;
                }
            },
            value);
    }
    return j.dump();
}

} // namespace cradle
