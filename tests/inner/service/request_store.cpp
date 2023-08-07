#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/service/request_store.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][service][request_store]";

request_uuid
make_test_uuid(int ext)
{
    return request_uuid{fmt::format("{}-{:04d}", tag, ext)};
}

class mock_storage : public secondary_storage_intf
{
 public:
    void
    reset(service_config const& config) override;

    cppcoro::task<blob>
    read(std::string key) override;

    cppcoro::task<void>
    write(std::string key, blob value) override;

    int
    size() const
    {
        return static_cast<int>(storage_.size());
    }

 private:
    std::map<std::string, blob> storage_;
};

void
mock_storage::reset(service_config const& config)
{
    throw not_implemented_error();
}

cppcoro::task<blob>
mock_storage::read(std::string key)
{
    auto it = storage_.find(key);
    co_return it != storage_.end() ? it->second : blob();
}

cppcoro::task<void>
mock_storage::write(std::string key, blob value)
{
    storage_[key] = value;
    co_return;
}

static auto add2 = [](int a, int b) { return a + b; };

} // namespace

TEST_CASE("get_request_key()", tag)
{
    request_props<caching_level_type::full> props{make_test_uuid(100)};
    auto req0{rq_function_erased(props, add2, 1, 2)};
    auto req1{rq_function_erased(props, add2, 1, 3)};

    std::string key0{get_request_key(req0)};
    std::string key1{get_request_key(req1)};

    REQUIRE(key0 != "");
    REQUIRE(key0.size() == 64);
    REQUIRE(key0 != key1);
}

TEST_CASE("store request in storage", tag)
{
    request_props<caching_level_type::full> props{make_test_uuid(200)};
    mock_storage storage;

    auto req0{rq_function_erased(props, add2, 1, 2)};
    cppcoro::sync_wait(store_request(req0, storage));

    REQUIRE(storage.size() == 1);

    auto req1{rq_function_erased(props, add2, 1, 3)};
    cppcoro::sync_wait(store_request(req1, storage));

    REQUIRE(storage.size() == 2);
}

TEST_CASE("load request from storage (hit)", tag)
{
    request_props<caching_level_type::full> props{make_test_uuid(300)};
    auto req_written{rq_function_erased(props, add2, 1, 2)};
    mock_storage storage;
    cppcoro::sync_wait(store_request(req_written, storage));

    using Req = decltype(req_written);
    std::string key{get_request_key(req_written)};
    auto req_read = cppcoro::sync_wait(load_request<Req>(key, storage));
    REQUIRE(req_read == req_written);
}

TEST_CASE("load request from storage (miss)", tag)
{
    request_props<caching_level_type::full> props{make_test_uuid(400)};
    auto req_written{rq_function_erased(props, add2, 1, 2)};
    auto req_not_written{rq_function_erased(props, add2, 1, 3)};
    mock_storage storage;
    cppcoro::sync_wait(store_request(req_written, storage));

    using Req = decltype(req_not_written);
    std::string key{get_request_key(req_not_written)};
    REQUIRE_THROWS_AS(
        cppcoro::sync_wait(load_request<Req>(key, storage)), not_found_error);
}
