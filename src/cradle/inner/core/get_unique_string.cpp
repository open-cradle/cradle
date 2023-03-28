#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/unique_hash.h>

namespace cradle {

std::string
get_unique_string(id_interface const& id)
{
    unique_hasher hasher;
    id.update_hash(hasher);
    return hasher.get_string();
}

} // namespace cradle
