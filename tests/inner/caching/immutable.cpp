#include <cradle/inner/caching/immutable.h>

#include <sstream>

#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

using namespace cradle;

namespace {

cppcoro::shared_task<int>
test_task(
    detail::immutable_cache_impl& cache,
    id_interface const& key,
    int the_answer)
{
    // TODO test cache_task_wrapper() itself, instead of copied code
    record_immutable_cache_value(cache, key, sizeof(int));
    co_return the_answer;
}

template<class Value>
Value
await_cache_value(immutable_cache_ptr<Value>& ptr)
{
    return cppcoro::sync_wait(ptr.task());
}

} // namespace

TEST_CASE("basic immutable cache usage", "[immutable_cache]")
{
    std::string key0 = get_unique_string(make_id(0));
    std::string key1 = get_unique_string(make_id(1));

    immutable_cache cache;
    {
        INFO("Cache reset() and is_initialized() work as expected.");
        REQUIRE(!cache.is_initialized());
        cache.reset(immutable_cache_config{1024});
        REQUIRE(cache.is_initialized());
        cache.reset();
        REQUIRE(!cache.is_initialized());
        cache.reset(immutable_cache_config{1024});
        REQUIRE(cache.is_initialized());
    }

    INFO(
        "The first time an immutable_cache_ptr is attached to a new key, "
        "its :create_job callback is invoked.");
    bool p_needed_creation = false;
    auto p_key = make_captured_id(0);
    immutable_cache_ptr<int> p(
        cache, p_key, [&](detail::immutable_cache_impl&, captured_id const&) {
            p_needed_creation = true;
            return test_task(*cache.impl, *p_key, 42);
        });
    REQUIRE(p_needed_creation);
    // Also check that all the ptr accessors work.
    REQUIRE(p.is_loading());
    REQUIRE(!p.is_ready());
    REQUIRE(!p.is_failed());
    REQUIRE(p.key() == make_id(0));

    {
        INFO("get_cache_snapshot reflects that entry 0 is loading.");
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::LOADING, 0}}, {}}));
    }

    bool q_needed_creation = false;
    auto q_key = make_captured_id(1);
    auto q = std::make_unique<immutable_cache_ptr<int>>(
        cache, q_key, [&](detail::immutable_cache_impl&, captured_id const&) {
            q_needed_creation = true;
            return test_task(*cache.impl, *q_key, 112);
        });
    {
        INFO(
            "The first time an immutable_cache_ptr is attached to a new key, "
            "its :create_job callback is invoked.");
        REQUIRE(q_needed_creation);
    }
    // Also check all that all the ptr accessors work.
    REQUIRE(!q->is_ready());
    REQUIRE(q->key() == make_id(1));

    {
        INFO(
            "get_cache_snapshot reflects that there are two entries loading.");
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::LOADING, 0},
                 {key1, immutable_cache_entry_state::LOADING, 0}},
                {}}));
    }

    // p and r have the same id
    bool r_needed_creation = false;
    auto r_key = make_captured_id(0);
    immutable_cache_ptr<int> r(
        cache, r_key, [&](detail::immutable_cache_impl&, captured_id const&) {
            r_needed_creation = true;
            return test_task(*cache.impl, *r_key, 42);
        });
    {
        INFO(
            "The second time an immutable_cache_ptr is attached to a key, "
            "its :create_job callback is NOT invoked.");
        REQUIRE(!r_needed_creation);
    }

    {
        INFO("get_cache_snapshot shows no change.");
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::LOADING, 0},
                 {key1, immutable_cache_entry_state::LOADING, 0}},
                {}}));
    }

    {
        INFO(
            "When a cache pointer p is waited on, this triggers production of "
            "the value. The value is correctly received and reflected in the "
            "cache snapshot.");
        REQUIRE(await_cache_value(p) == 42);
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::READY, sizeof(int)},
                 {key1, immutable_cache_entry_state::LOADING, 0}},
                {}}));
        REQUIRE(p.is_ready());
        REQUIRE(q->is_loading());
        REQUIRE(r.is_ready());
    }

    {
        INFO(
            "Waiting on r (with the same id as p) gives the same result, "
            "but otherwise nothing changes.");
        REQUIRE(await_cache_value(r) == 42);
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::READY, sizeof(int)},
                 {key1, immutable_cache_entry_state::LOADING, 0}},
                {}}));
        REQUIRE(p.is_ready());
        REQUIRE(q->is_loading());
        REQUIRE(r.is_ready());
    }

    {
        INFO("Waiting on q (different id) gives the expected results.");
        REQUIRE(await_cache_value(*q) == 112);
        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::READY, sizeof(int)},
                 {key1, immutable_cache_entry_state::READY, sizeof(int)}},
                {}}));
        REQUIRE(p.is_ready());
        REQUIRE(q->is_ready());
        REQUIRE(r.is_ready());
    }

    {
        INFO("Deleting q will put it into the eviction list.");

        q.reset();

        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::READY, sizeof(int)}},
                {{key1, immutable_cache_entry_state::READY, sizeof(int)}}}));
    }

    {
        INFO(
            "Clearing unused entries in the cache will clear out q's old "
            "value.");

        clear_unused_entries(cache);

        REQUIRE(
            get_cache_snapshot(cache)
            == (immutable_cache_snapshot{
                {{key0, immutable_cache_entry_state::READY, sizeof(int)}},
                {}}));
    }
}

namespace {

cppcoro::shared_task<std::string>
one_kb_string_task(
    detail::immutable_cache_impl& cache, id_interface const& key, char content)
{
    // TODO test cache_task_wrapper() itself, instead of copied code
    std::string result(1024, content);
    record_immutable_cache_value(cache, key, deep_sizeof(result));
    co_return result;
}

} // namespace

TEST_CASE("immutable cache LRU eviction", "[immutable_cache]")
{
    // Initialize the cache with 1.5kB of space for unused data.
    immutable_cache cache(immutable_cache_config{1536});

    // Declare an interest in ID(1).
    bool p_needed_creation = false;
    auto p_key = make_captured_id(1);
    auto p = std::make_unique<immutable_cache_ptr<std::string>>(
        cache, p_key, [&](detail::immutable_cache_impl&, captured_id const&) {
            p_needed_creation = true;
            return one_kb_string_task(*cache.impl, *p_key, 'a');
        });
    REQUIRE(p_needed_creation);
    REQUIRE(await_cache_value(*p) == std::string(1024, 'a'));

    // Declare an interest in ID(2).
    bool q_needed_creation = false;
    auto q_key = make_captured_id(2);
    auto q = std::make_unique<immutable_cache_ptr<std::string>>(
        cache, q_key, [&](detail::immutable_cache_impl&, captured_id const&) {
            q_needed_creation = true;
            return one_kb_string_task(*cache.impl, *q_key, 'b');
        });
    REQUIRE(q_needed_creation);
    REQUIRE(await_cache_value(*q) == std::string(1024, 'b'));

    // Revoke interest in both IDs.
    // Since only one will fit in the cache, this should evict ID(1).
    p.reset();
    q.reset();

    // If we redeclare interest in ID(1), it should require creation.
    bool r_needed_creation = false;
    auto r_key = make_captured_id(1);
    immutable_cache_ptr<std::string> r(
        cache, r_key, [&](detail::immutable_cache_impl&, captured_id const&) {
            r_needed_creation = true;
            return one_kb_string_task(*cache.impl, *r_key, 'a');
        });
    REQUIRE(r_needed_creation);
    REQUIRE(!r.is_ready());
    REQUIRE(await_cache_value(r) == std::string(1024, 'a'));

    // If we redeclare interest in ID(2), it should NOT require creation.
    bool s_needed_creation = false;
    auto s_key = make_captured_id(2);
    immutable_cache_ptr<std::string> s(
        cache, s_key, [&](detail::immutable_cache_impl&, captured_id const&) {
            s_needed_creation = true;
            return one_kb_string_task(*cache.impl, *s_key, 'b');
        });
    REQUIRE(!s_needed_creation);
    REQUIRE(s.is_ready());
    REQUIRE(await_cache_value(s) == std::string(1024, 'b'));
}
