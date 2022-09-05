#include "id_support.h"

/*
 * Support for the id.cpp benchmarks, put in a separate translation unit to
 * prevent inlining from optimizing everything away.
 */

using namespace cradle;

namespace {

struct empty0
{
};

struct empty1
{
};

template<typename T>
struct my_struct
{
};

template<typename TL, typename TR>
bool
operator==(my_struct<TL> const&, my_struct<TR> const&)
{
    return true;
}

template<typename TL, typename TR>
bool
operator<(my_struct<TL> const&, my_struct<TR> const&)
{
    return false;
}

template<typename T>
size_t
hash_value(my_struct<T> const& x)
{
    return 0;
}

template<typename T>
void
update_unique_hash(unique_hasher& hasher, my_struct<T> const& x)
{
}

} // namespace

namespace cradle {

id_interface const&
get_my_struct_id0()
{
    // S0 will have a longish typeinfo.name()
    using S0 = my_struct<my_struct<my_struct<my_struct<empty0>>>>;
    static simple_id<S0> s0;
    return s0;
}

id_interface const&
get_my_struct_id1()
{
    using S1 = my_struct<my_struct<my_struct<my_struct<empty1>>>>;
    static simple_id<S1> s1;
    return s1;
}

} // namespace cradle
