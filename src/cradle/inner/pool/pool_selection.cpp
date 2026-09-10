#include <cradle/inner/pool/pool_selection.h>

namespace cradle {

std::optional<std::string>
select_pool_name(
    std::optional<std::string> const& request_pool,
    std::optional<std::string> const& context_default_pool)
{
    if (request_pool)
    {
        return request_pool;
    }
    return context_default_pool;
}

} // namespace cradle
