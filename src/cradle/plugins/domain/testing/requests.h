#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_REQUESTS_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_REQUESTS_H

#include <cstddef>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/plugins/domain/testing/context.h>

namespace cradle {

template<caching_level_type Level>
using testing_request_props
    = request_props<Level, true, true, testing_request_context>;

cppcoro::task<blob>
make_some_blob(testing_request_context ctx, std::size_t size);

template<caching_level_type Level>
auto
rq_make_some_blob(std::size_t size)
{
    return rq_function_erased_coro<blob>(
        testing_request_props<Level>(
            request_uuid{"make_some_blob"}, std::string{"make_some_blob"}),
        make_some_blob,
        size);
}

} // namespace cradle

#endif
