#include <catch2/catch.hpp>

#include "../../inner-dll/v1/adder_v1_defs.h"
#include "../../inner-dll/v1/adder_v1_impl.h"
#include "../../support/inner_service.h"
#include <cradle/inner/encodings/msgpack_value.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/value.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/test_dlls_dir.h>

namespace cradle {

namespace {

static char const tag[] = "[inner][resolve][seri_req_msgpack]";

containment_data const coro_v1_containment{
    request_uuid{coro_v1p_uuid}, get_test_dlls_dir(), "test_inner_dll_v1"};

void
test_request(auto const& saved_req)
{
    using req_type = std::remove_cvref_t<decltype(saved_req)>;
    auto resources{make_inner_test_resources("")};
    auto registry{resources->get_seri_registry()};
    seri_catalog cat{registry};
    if constexpr (!is_value_request_v<req_type>)
    {
        cat.register_resolver(saved_req);
    }

    bool allow_blob_files{false};
    auto seri = serialize_value(saved_req, allow_blob_files);

    auto loaded_req = deserialize_value<req_type>(seri);

    REQUIRE(loaded_req == saved_req);
}

} // namespace

TEST_CASE("serialize function_request using msgpack", tag)
{
    test_request(rq_test_coro_v1n_impl(&coro_v1_containment, 10, 5));
}

TEST_CASE("serialize function_request with retrier using msgpack", tag)
{
    using props_type = request_props<
        caching_level_type::none,
        request_function_t::plain,
        false,
        default_retrier>;
    auto saved_req = rq_function(
        props_type{request_uuid{"tmp_uuid"}}, adder_v1_func, 10, 5);
    test_request(saved_req);
}

TEST_CASE("serialize value_request<string> using msgpack", tag)
{
    test_request(rq_value(std::string{"test1"}));
}

TEST_CASE("serialize value_request<blob> using msgpack", tag)
{
    test_request(rq_value(make_blob("test2")));
}

} // namespace cradle
