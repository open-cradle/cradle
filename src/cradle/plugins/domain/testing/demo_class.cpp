#include <cradle/inner/core/hash.h>
#include <cradle/inner/core/unique_hash.h>
#include <cradle/plugins/domain/testing/demo_class.h>

namespace cradle {

bool
operator==(demo_class const& a, demo_class const& b)
{
    return a.get_x() == b.get_x();
}

bool
operator<(demo_class const& a, demo_class const& b)
{
    return a.get_x() < b.get_x();
}

std::size_t
hash_value(demo_class const& val)
{
    return invoke_hash(val.get_x());
}

void
update_unique_hash(unique_hasher& hasher, demo_class const& val)
{
    update_unique_hash(hasher, val.get_x());
}

} // namespace cradle
