#include <cradle/inner/pool/job_spec_builder.h>

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/pool/job_spec.h>
#include <cradle/inner/pool/job_types.h>
#include <cradle/inner/storage/cas_intf.h>
#include <cradle/inner/storage/content_helpers.h>
#include <cradle/inner/storage/digest.h>
#include <cradle/inner/storage/memory_cas.h>

using namespace cradle;

static char const tag[] = "[unit][inner][pool][job_spec_builder]";

TEST_CASE("build_job_input - at or below boundary is conveyed inline", tag)
{
    auto cas = make_memory_cas("test_cas");

    SECTION("below boundary")
    {
        blob const content = make_blob("small");
        std::size_t const boundary = content.size() + 8;

        job_input const ji
            = cppcoro::sync_wait(build_job_input(*cas, content, boundary));

        REQUIRE(std::holds_alternative<inline_input>(ji));
        REQUIRE(to_string(std::get<inline_input>(ji).bytes) == "small");
    }

    SECTION("exactly at boundary pins the <= rule")
    {
        blob const content = make_blob("exact-size");
        std::size_t const boundary = content.size();

        job_input const ji
            = cppcoro::sync_wait(build_job_input(*cas, content, boundary));

        REQUIRE(std::holds_alternative<inline_input>(ji));
        REQUIRE(to_string(std::get<inline_input>(ji).bytes) == "exact-size");
    }
}

TEST_CASE(
    "build_job_input - above boundary is conveyed by digest with content "
    "present in the CAS before return",
    tag)
{
    auto cas = make_memory_cas("test_cas");

    blob const content = make_blob("this payload exceeds the boundary");
    std::size_t const boundary = content.size() - 1;

    job_input const ji
        = cppcoro::sync_wait(build_job_input(*cas, content, boundary));

    REQUIRE(std::holds_alternative<digest_input>(ji));
    digest const d = std::get<digest_input>(ji).content_digest;

    // The digest names the exact bytes that were classified.
    REQUIRE(d == digest_of(content));

    // The content is already present in the CAS before the job runs.
    REQUIRE(cppcoro::sync_wait(cas->exists(d)));

    auto const retrieved = cppcoro::sync_wait(cas->get(d));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "this payload exceeds the boundary");
}

TEST_CASE("build_job_input - the boundary controls the classification", tag)
{
    auto cas = make_memory_cas("test_cas");

    blob const content = make_blob("classify me");

    // A small boundary forces digest conveyance.
    job_input const by_digest = cppcoro::sync_wait(
        build_job_input(*cas, content, content.size() - 1));
    REQUIRE(std::holds_alternative<digest_input>(by_digest));

    // A large boundary forces inline conveyance for the same input.
    job_input const inline_conveyed = cppcoro::sync_wait(
        build_job_input(*cas, content, content.size() + 100));
    REQUIRE(std::holds_alternative<inline_input>(inline_conveyed));
    REQUIRE(
        to_string(std::get<inline_input>(inline_conveyed).bytes)
        == "classify me");
}

TEST_CASE("build_job_inputs - preserves argument order", tag)
{
    auto cas = make_memory_cas("test_cas");

    blob const a = make_blob("a");
    blob const big_b = make_blob("bbbbbbbbbbbbbbbb");
    blob const c = make_blob("c");
    blob const big_d = make_blob("dddddddddddddddd");

    std::vector<blob> const inputs{a, big_b, c, big_d};

    // Boundary between the short and the long inputs, so the sequence
    // alternates inline / digest / inline / digest.
    std::size_t const boundary = 4;

    std::vector<job_input> const result
        = cppcoro::sync_wait(build_job_inputs(*cas, inputs, boundary));

    REQUIRE(result.size() == 4);

    REQUIRE(std::holds_alternative<inline_input>(result[0]));
    REQUIRE(to_string(std::get<inline_input>(result[0]).bytes) == "a");

    REQUIRE(std::holds_alternative<digest_input>(result[1]));
    REQUIRE(
        std::get<digest_input>(result[1]).content_digest == digest_of(big_b));

    REQUIRE(std::holds_alternative<inline_input>(result[2]));
    REQUIRE(to_string(std::get<inline_input>(result[2]).bytes) == "c");

    REQUIRE(std::holds_alternative<digest_input>(result[3]));
    REQUIRE(
        std::get<digest_input>(result[3]).content_digest == digest_of(big_d));
}

TEST_CASE("build_job_spec - assembles the spec from its arguments", tag)
{
    auto cas = make_memory_cas("test_cas");

    provider_id const provider = "provider-uuid";
    request_key const key = "leaf/key/spec";
    context_id const context = "ctx-1";

    blob const small = make_blob("in");
    blob const large = make_blob("oversized-argument-bytes");
    std::vector<blob> const inputs{small, large};

    std::size_t const boundary = 4;

    job_spec const spec = cppcoro::sync_wait(
        build_job_spec(*cas, provider, key, context, inputs, boundary));

    REQUIRE(spec.provider == provider);
    REQUIRE(spec.key == key);
    REQUIRE(spec.context == context);

    REQUIRE(spec.inputs.size() == 2);

    REQUIRE(std::holds_alternative<inline_input>(spec.inputs[0]));
    REQUIRE(to_string(std::get<inline_input>(spec.inputs[0]).bytes) == "in");

    REQUIRE(std::holds_alternative<digest_input>(spec.inputs[1]));
    digest const d = std::get<digest_input>(spec.inputs[1]).content_digest;
    REQUIRE(d == digest_of(large));

    // Digest-conveyed content is present in the CAS after assembly.
    auto const retrieved = cppcoro::sync_wait(cas->get(d));
    REQUIRE(retrieved.has_value());
    REQUIRE(to_string(*retrieved) == "oversized-argument-bytes");
}
