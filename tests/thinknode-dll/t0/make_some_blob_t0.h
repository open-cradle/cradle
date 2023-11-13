#ifndef CRADLE_TESTS_THINKNODE_DLL_T0_MAKE_SOME_BLOB_T0_H
#define CRADLE_TESTS_THINKNODE_DLL_T0_MAKE_SOME_BLOB_T0_H

#include <string>

#include "make_some_blob_t0_defs.h"
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/requests/value.h>
#include <cradle/thinknode/request_props.h>

namespace cradle {

// Creates a proxy request to be resolved remotely.
// Value comes from a value request; no other type of subrequest possible.
// Remote caching level is "full".
auto
rq_proxy_make_test_blob(std::string payload)
{
    request_uuid uuid{make_some_blob_t0_base_uuid};
    uuid.set_level(caching_level_type::full);
    return rq_proxy<blob>(
        thinknode_proxy_props(std::move(uuid), make_some_blob_t0_title),
        rq_value(std::move(payload)));
}

} // namespace cradle

#endif
