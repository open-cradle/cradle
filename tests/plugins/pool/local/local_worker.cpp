#include <cradle/plugins/pool/local/local_worker.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>

#include "../../../support/inner_service.h"
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/msgpack_value.h>
#include <cradle/inner/pool/job_record.h>
#include <cradle/inner/pool/job_spec.h>
#include <cradle/inner/pool/job_types.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/service/resources.h>
#include <cradle/inner/storage/ac_intf.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/content_helpers.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/memory_ac.h>
#include <cradle/inner/storage/memory_cas.h>
#include <cradle/inner/storage/memory_mutable_store.h>
#include <cradle/inner/storage/mutable_store_intf.h>
#include <cradle/plugins/domain/testing/demo_class.h>

using namespace cradle;

namespace {

char const tag[] = "[plugins][pool][local_worker]";

// A pooled leaf provider that combines two demo_class inputs so that the
// result reflects both. Non-introspective and non-retrying, matching the
// request shape the worker reassembles from a provider identity and inputs.
cppcoro::task<demo_class>
combine_demo(context_intf&, demo_class a, demo_class b)
{
    co_return demo_class{a.get_x() + b.get_x(), a.get_y()};
}

// A pooled leaf provider that always throws, to exercise the worker's failure
// path.
cppcoro::task<demo_class>
failing_demo(context_intf&, demo_class)
{
    throw std::runtime_error{"intentional provider failure"};
}

using worker_props
    = request_props<caching_level_type::none, request_function_t::coro>;

// Serializes a value exactly as the pool input path does before a job runs:
// resolve_on_pool serializes each resolved argument value with
// serialize_value, and the worker later deserializes those same bytes.
blob
to_input(demo_class const& value)
{
    bool constexpr allow_blob_files{false};
    return serialize_value(value, allow_blob_files);
}

} // namespace

TEST_CASE(
    "local_worker records succeeded and stores the result and its "
    "association",
    tag)
{
    auto resources{make_inner_test_resources()};
    auto cas{make_memory_cas("pool_cas")};
    auto ac{make_memory_ac("pool_ac")};
    auto store{make_memory_mutable_store("pool_store")};

    // Register the provider so find_resolver(provider) succeeds and the
    // reassembled request resolves through the reused seri machinery.
    seri_catalog cat{resources->get_seri_registry()};
    auto req{rq_function(
        worker_props{request_uuid{"local_worker/combine"}},
        combine_demo,
        demo_class{},
        demo_class{})};
    cat.register_resolver(req);
    std::string const provider{req.get_essentials()->uuid_str};

    auto worker{make_local_worker(*resources, *cas, *ac, *store, "local")};

    demo_class const a{3, make_blob("alpha")};
    demo_class const b{4, make_blob("beta")};

    job_spec spec;
    spec.provider = provider;
    spec.key = "leaf/combine";
    spec.context = "";
    spec.inputs.push_back(inline_input{to_input(a)});
    spec.inputs.push_back(inline_input{to_input(b)});

    job_id const id{"job-combine"};
    cppcoro::sync_wait(worker->execute(spec, id));

    std::optional<mutable_value> const stored{
        cppcoro::sync_wait(store->get(job_record_key(id)))};
    REQUIRE(stored.has_value());
    job_record const record{deserialize_job_record(*stored)};
    REQUIRE(record.status == job_status::succeeded);
    REQUIRE(!record.output_digest.empty());

    // The CAS holds the result content named by the terminal record's digest,
    // and it deserializes to the expected combined value.
    std::optional<blob> const content{
        cppcoro::sync_wait(cas->get(record.output_digest))};
    REQUIRE(content.has_value());
    demo_class const result{deserialize_value<demo_class>(*content)};
    REQUIRE(result.get_x() == 7);

    // The AC associates the request key with the result digest.
    std::optional<digest> const associated{
        cppcoro::sync_wait(ac->get(spec.key))};
    REQUIRE(associated.has_value());
    REQUIRE(*associated == record.output_digest);
}

TEST_CASE("local_worker loads inline and digest-conveyed inputs", tag)
{
    auto resources{make_inner_test_resources()};
    auto cas{make_memory_cas("pool_cas")};
    auto ac{make_memory_ac("pool_ac")};
    auto store{make_memory_mutable_store("pool_store")};

    seri_catalog cat{resources->get_seri_registry()};
    auto req{rq_function(
        worker_props{request_uuid{"local_worker/combine-mixed"}},
        combine_demo,
        demo_class{},
        demo_class{})};
    cat.register_resolver(req);
    std::string const provider{req.get_essentials()->uuid_str};

    auto worker{make_local_worker(*resources, *cas, *ac, *store, "local")};

    demo_class const inline_val{5, make_blob("small")};
    demo_class const digest_val{
        6, make_blob("a larger payload conveyed by digest")};

    // The first input travels inline in the job; the second is placed in the
    // CAS and referenced by its content digest.
    digest const input_digest{
        cppcoro::sync_wait(put_content(*cas, to_input(digest_val)))};

    job_spec spec;
    spec.provider = provider;
    spec.key = "leaf/mixed";
    spec.context = "";
    spec.inputs.push_back(inline_input{to_input(inline_val)});
    spec.inputs.push_back(digest_input{input_digest});

    job_id const id{"job-mixed"};
    cppcoro::sync_wait(worker->execute(spec, id));

    std::optional<mutable_value> const stored{
        cppcoro::sync_wait(store->get(job_record_key(id)))};
    REQUIRE(stored.has_value());
    job_record const record{deserialize_job_record(*stored)};
    REQUIRE(record.status == job_status::succeeded);
    REQUIRE(!record.output_digest.empty());

    // The result reflects both the inline and the digest-conveyed input.
    std::optional<blob> const content{
        cppcoro::sync_wait(cas->get(record.output_digest))};
    REQUIRE(content.has_value());
    demo_class const result{deserialize_value<demo_class>(*content)};
    REQUIRE(result.get_x() == 11);

    std::optional<digest> const associated{
        cppcoro::sync_wait(ac->get(spec.key))};
    REQUIRE(associated.has_value());
    REQUIRE(*associated == record.output_digest);
}

TEST_CASE(
    "local_worker records failed without a result or association on provider "
    "failure",
    tag)
{
    auto resources{make_inner_test_resources()};
    auto cas{make_memory_cas("pool_cas")};
    auto ac{make_memory_ac("pool_ac")};
    auto store{make_memory_mutable_store("pool_store")};

    seri_catalog cat{resources->get_seri_registry()};
    auto req{rq_function(
        worker_props{request_uuid{"local_worker/failing"}},
        failing_demo,
        demo_class{})};
    cat.register_resolver(req);
    std::string const provider{req.get_essentials()->uuid_str};

    auto worker{make_local_worker(*resources, *cas, *ac, *store, "local")};

    demo_class const a{1, make_blob("input")};

    job_spec spec;
    spec.provider = provider;
    spec.key = "leaf/failing";
    spec.context = "";
    spec.inputs.push_back(inline_input{to_input(a)});

    job_id const id{"job-failing"};
    cppcoro::sync_wait(worker->execute(spec, id));

    std::optional<mutable_value> const stored{
        cppcoro::sync_wait(store->get(job_record_key(id)))};
    REQUIRE(stored.has_value());
    job_record const record{deserialize_job_record(*stored)};
    REQUIRE(record.status == job_status::failed);
    REQUIRE(!record.error.empty());
    REQUIRE(record.output_digest.empty());

    // A failure is never observable as a success: no association is recorded.
    std::optional<digest> const associated{
        cppcoro::sync_wait(ac->get(spec.key))};
    REQUIRE(!associated.has_value());
}
