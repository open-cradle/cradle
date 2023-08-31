#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

std::vector<std::string>
seri_catalog::get_all_uuid_strs() const
{
    std::vector<std::string> res;
    for (auto const& it : map_)
    {
        res.push_back(it.first);
    }
    return res;
}

} // namespace cradle
