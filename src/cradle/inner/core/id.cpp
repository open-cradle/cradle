#include <cradle/inner/core/id.h>
#include <typeinfo>

namespace cradle {

inline bool
types_match(id_interface const& a, id_interface const& b)
{
    // This is equivalent to `typeid(a) == typeid(b)` but is supposed to be
    // faster in cases where the result is often true.
    return typeid(a).name() == typeid(b).name() || typeid(a) == typeid(b);
}

bool
operator<(id_interface const& a, id_interface const& b)
{
    return typeid(a).before(typeid(b))
           || (types_match(a, b) && a.less_than(b));
}

bool
operator==(captured_id const& a, captured_id const& b)
{
    return a.is_initialized() == b.is_initialized()
           && (!a.is_initialized() || *a == *b);
}
bool
operator!=(captured_id const& a, captured_id const& b)
{
    return !(a == b);
}
bool
operator<(captured_id const& a, captured_id const& b)
{
    return b.is_initialized() && (!a.is_initialized() || *a < *b);
}

} // namespace cradle
