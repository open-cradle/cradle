#include <iostream>

#include <boost/dll.hpp>
#include <cereal/types/map.hpp>

#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>
#include <cradle/thinknode/iss_req.h>

namespace cradle {

static seri_catalog&
get_catalog()
{
    static seri_catalog the_catalog;
    return the_catalog;
}

// To be called when the DLL is loaded
extern "C" BOOST_SYMBOL_EXPORT void
CRADLE_init()
{
    auto& cat{get_catalog()};
    auto sample_thinknode_info{
        make_thinknode_type_info_with_nil_type(thinknode_nil_type())};
    cat.register_resolver(rq_post_iss_object_v1_impl(
        "sample context id", sample_thinknode_info, blob()));
}

extern "C" BOOST_SYMBOL_EXPORT seri_catalog*
CRADLE_get_catalog()
{
    return &get_catalog();
}

} // namespace cradle
