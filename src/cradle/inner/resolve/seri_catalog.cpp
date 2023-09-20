#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

seri_catalog::seri_catalog(bool is_static)
    : cat_id_{is_static ? catalog_id::get_static_id() : catalog_id{}}
{
}

void
seri_catalog::alloc_dll_id()
{
    cat_id_ = catalog_id::alloc_dll_id();
}

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
