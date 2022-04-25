#include <cradle/typing/service/core.h>

#include <cppcoro/sync_wait.hpp>

#include <filesystem>

#include <cradle/typing/service/internals.h>
#include <cradle/typing/utilities/concurrency_testing.h>

#include <cradle/typing/utilities/testing.h>

using namespace cradle;

TEST_CASE("HTTP requests", "[service][core]")
{
    service_core core;
    init_test_service(core);

    auto async_response = async_http_request(
        core,
        make_get_request(
            "http://postman-echo.com/get?color=navy", http_header_list()));

    auto response = cppcoro::sync_wait(async_response);
    REQUIRE(response.status_code == 200);
    auto body = parse_json_response(response);
    REQUIRE(
        get_field(cast<dynamic_map>(body), "args")
        == dynamic({{"color", "navy"}}));
}

TEST_CASE("small value disk caching", "[service][core]")
{
    service_core core;
    init_test_service(core);

    int execution_count = 0;
    auto counted_task = [&](int answer) -> cppcoro::task<dynamic> {
        ++execution_count;
        co_return dynamic(integer(answer));
    };

    {
        auto key = make_id("id_12");
        auto result = disk_cached<dynamic>(
            core, key, [&] { return counted_task(12); });
        REQUIRE(cppcoro::sync_wait(result) == dynamic(integer(12)));
        REQUIRE(execution_count == 1);
    }
    {
        auto key = make_id("id_42");
        auto result = disk_cached<dynamic>(
            core, key, [&] { return counted_task(42); });
        REQUIRE(cppcoro::sync_wait(result) == dynamic(integer(42)));
        REQUIRE(execution_count == 2);
    }
    // Data is written to the disk cache in a background thread, so we need to
    // wait for that to finish.
    REQUIRE(occurs_soon([&] {
        return core.inner_internals().disk_write_pool.get_tasks_total() == 0;
    }));
    // Now redo the 'id_12' task to see that it's not actually rerun.
    {
        auto key = make_id("id_12");
        auto result = disk_cached<dynamic>(
            core, key, [&] { return counted_task(12); });
        REQUIRE(cppcoro::sync_wait(result) == dynamic(integer(12)));
        REQUIRE(execution_count == 2);
    }
}

TEST_CASE("large value disk caching", "[service][core]")
{
    service_core core;
    init_test_service(core);

    auto generate_random_data = [](uint32_t seed) {
        std::vector<integer> result;
        std::minstd_rand eng(seed);
        std::uniform_int_distribution<integer> dist(0, 0x1'0000'0000);
        for (int i = 0; i < 256; ++i)
            result.push_back(dist(eng));
        return result;
    };

    int execution_count = 0;
    auto counted_task = [&](uint32_t seed) -> cppcoro::task<dynamic> {
        ++execution_count;
        co_return to_dynamic(generate_random_data(seed));
    };

    {
        auto key = make_id("id_12");
        auto result = disk_cached<dynamic>(
            core, key, [&] { return counted_task(12); });
        REQUIRE(
            cppcoro::sync_wait(result)
            == to_dynamic(generate_random_data(12)));
        REQUIRE(execution_count == 1);
    }
    {
        auto key = make_id("id_42");
        auto result = disk_cached<dynamic>(
            core, key, [&] { return counted_task(42); });
        REQUIRE(
            cppcoro::sync_wait(result)
            == to_dynamic(generate_random_data(42)));
        REQUIRE(execution_count == 2);
    }
    // Data is written to the disk cache in a background thread, so we need to
    // wait for that to finish.
    REQUIRE(occurs_soon([&] {
        return core.inner_internals().disk_write_pool.get_tasks_total() == 0;
    }));
    // Now redo the 'id_12' task to see that it's not actually rerun.
    {
        auto key = make_id("id_12");
        auto result = disk_cached<dynamic>(
            core, key, [&] { return counted_task(12); });
        REQUIRE(
            cppcoro::sync_wait(result)
            == to_dynamic(generate_random_data(12)));
        REQUIRE(execution_count == 2);
    }
}

TEST_CASE("cached tasks", "[service][core]")
{
    service_core core;
    init_test_service(core);

    int execution_count = 0;
    auto counted_task = [&](int answer) -> cppcoro::task<integer> {
        ++execution_count;
        co_return integer(answer);
    };

    {
        auto result = cached<integer>(
            core, make_captured_id(12), [&](id_interface const&) {
                return counted_task(12);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(12));
        REQUIRE(execution_count == 1);
    }
    {
        auto result = cached<integer>(
            core, make_captured_id(42), [&](id_interface const&) {
                return counted_task(42);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(42));
        REQUIRE(execution_count == 2);
    }
    // Now redo the '12' task to see that it's not actually rerun.
    {
        auto result = cached<integer>(
            core, make_captured_id(12), [&](id_interface const&) {
                return counted_task(12);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(12));
        REQUIRE(execution_count == 2);
    }
}

TEST_CASE("lazily generated cached tasks", "[service][core]")
{
    service_core core;
    init_test_service(core);

    int execution_count = 0;
    auto counted_task = [&](int answer) -> cppcoro::task<integer> {
        ++execution_count;
        co_return integer(answer);
    };

    {
        auto result = cached<integer>(
            core,
            make_captured_id(12),
            [&](id_interface const&) -> cppcoro::task<integer> {
                return counted_task(12);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(12));
        REQUIRE(execution_count == 1);
    }
    {
        auto result = cached<integer>(
            core,
            make_captured_id(42),
            [&](id_interface const&) -> cppcoro::task<integer> {
                return counted_task(42);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(42));
        REQUIRE(execution_count == 2);
    }
    // Now redo the '12' task to see that it's not actually rerun.
    {
        auto result = cached<integer>(
            core,
            make_captured_id(12),
            [&](id_interface const&) -> cppcoro::task<integer> {
                return counted_task(12);
            });
        REQUIRE(cppcoro::sync_wait(result) == integer(12));
        REQUIRE(execution_count == 2);
    }
}
