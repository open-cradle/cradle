#ifndef CRADLE_TESTS_THINKNODE_DLL_MAKE_SOME_BLOB_T0_MAKE_SOME_BLOB_T0_IMPL_H
#define CRADLE_TESTS_THINKNODE_DLL_MAKE_SOME_BLOB_T0_MAKE_SOME_BLOB_T0_IMPL_H

#include <cppcoro/task.hpp>

#include "make_some_blob_t0_defs.h"
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/requests/value.h>
#include <cradle/thinknode/request_props.h>

namespace cradle {

cppcoro::task<blob>
make_test_blob(context_intf& ctx, std::string payload);

// Creates a nonproxy request to be resolved locally or remotely.
// Value comes from a value request; no other type of subrequest possible.
// Caching level should be "full" if remote.
template<caching_level_type Level>
auto
rq_make_test_blob(std::string payload)
{
    request_uuid uuid{make_some_blob_t0_base_uuid};
    uuid.set_level(Level);
    using props_type = thinknode_request_props<Level>;
    return rq_function(
        props_type(std::move(uuid), make_some_blob_t0_title),
        make_test_blob,
        rq_value(std::move(payload)));
}

} // namespace cradle

#endif
