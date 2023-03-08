#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/core/unique_hash.h>

namespace cradle {

std::string
get_unique_string(id_interface const& id, unique_string_purpose purpose)
{
    unique_hasher hasher;
    id.update_hash(hasher);
    if (purpose != unique_string_purpose::RESULT)
    {
        update_unique_hash(hasher, static_cast<uint8_t>(purpose));
    }
    return hasher.get_string();
}

} // namespace cradle
