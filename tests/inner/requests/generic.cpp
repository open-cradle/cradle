#include <stdexcept>

#include <catch2/catch.hpp>

#include <cradle/inner/requests/cast_ctx.h>
#include <cradle/inner/requests/generic.h>

using namespace cradle;

namespace {

static char const tag[] = "[inner][requests][generic]";

class local_context_mixin : public local_context_intf
{
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) override
    {
        throw not_implemented_error();
    }

    void
    track_blob_file_writers() override
    {
        throw not_implemented_error();
    }

    void
    on_value_complete() override
    {
        throw not_implemented_error();
    }
};

class remote_context_mixin : public remote_context_intf
{
 public:
    remote_proxy&
    get_proxy() const override
    {
        throw not_implemented_error();
    }

    std::string const&
    domain_name() const override
    {
        throw not_implemented_error();
    }

    service_config
    make_config(bool need_record_lock) const override
    {
        throw not_implemented_error();
    }
};

class sync_context_mixin : public sync_context_intf
{
};

class async_context_mixin : public async_context_intf
{
 public:
    async_id
    get_id() const
    {
        throw not_implemented_error();
    }

    bool
    is_req() const
    {
        throw not_implemented_error();
    }

    std::size_t
    get_num_subs() const
    {
        throw not_implemented_error();
    }

    async_context_intf&
    get_sub(std::size_t ix)
    {
        throw not_implemented_error();
    }

    cppcoro::task<async_status>
    get_status_coro()
    {
        throw not_implemented_error();
    }

    cppcoro::task<void>
    request_cancellation_coro()
    {
        throw not_implemented_error();
    }
};

class my_local_only_context final : public local_context_mixin,
                                    public sync_context_mixin
{
 public:
    inner_resources&
    get_resources() override
    {
        throw not_implemented_error("my_local_only_context::get_resources()");
    }

    bool
    remotely() const override
    {
        throw not_implemented_error("my_local_only_context::remotely()");
    }

    bool
    is_async() const override
    {
        throw not_implemented_error("my_local_only_context::is_async()");
    }

    std::string const&
    domain_name() const override
    {
        throw not_implemented_error("my_local_only_context::domain_name()");
    }

    cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) override
    {
        throw not_implemented_error("my_local_only_context::schedule_after()");
    }
};
static_assert(ValidContext<my_local_only_context>);

class my_remote_only_context final : public remote_context_mixin,
                                     public sync_context_mixin
{
 public:
    inner_resources&
    get_resources() override
    {
        throw not_implemented_error("my_remote_only_context::get_resources()");
    }

    bool
    remotely() const override
    {
        throw not_implemented_error("my_remote_only_context::remotely()");
    }

    bool
    is_async() const override
    {
        throw not_implemented_error("my_remote_only_context::is_async()");
    }

    std::string const&
    domain_name() const override
    {
        throw not_implemented_error("my_remote_only_context::domain_name()");
    }

    cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) override
    {
        throw not_implemented_error(
            "my_remote_only_context::schedule_after()");
    }
};
static_assert(ValidContext<my_remote_only_context>);

class my_sync_only_context final : public local_context_mixin,
                                   public sync_context_mixin
{
 public:
    inner_resources&
    get_resources() override
    {
        throw not_implemented_error("my_sync_only_context::get_resources()");
    }

    bool
    remotely() const override
    {
        throw not_implemented_error("my_sync_only_context::remotely()");
    }

    bool
    is_async() const override
    {
        throw not_implemented_error("my_sync_only_context::is_async()");
    }

    std::string const&
    domain_name() const override
    {
        throw not_implemented_error("my_sync_only_context::domain_name()");
    }

    cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) override
    {
        throw not_implemented_error("my_sync_only_context::schedule_after()");
    }
};
static_assert(ValidContext<my_sync_only_context>);

class my_async_only_context final : public local_context_mixin,
                                    public async_context_mixin
{
 public:
    inner_resources&
    get_resources() override
    {
        throw not_implemented_error("my_async_only_context::get_resources()");
    }

    bool
    remotely() const override
    {
        throw not_implemented_error("my_async_only_context::remotely()");
    }

    bool
    is_async() const override
    {
        throw not_implemented_error("my_async_only_context::is_async()");
    }

    std::string const&
    domain_name() const override
    {
        throw not_implemented_error("my_async_only_context::domain_name()");
    }

    cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) override
    {
        throw not_implemented_error("my_async_only_context::schedule_after()");
    }
};
static_assert(ValidContext<my_async_only_context>);

constexpr int undef = 2;

class my_generic_context : public local_context_mixin,
                           public remote_context_mixin,
                           public sync_context_mixin,
                           public async_context_mixin
{
 public:
    my_generic_context(int remotely, int is_async)
        : remotely_{remotely}, is_async_{is_async}
    {
    }

    inner_resources&
    get_resources() override
    {
        throw not_implemented_error("my_generic_context::get_resources()");
    }

    bool
    remotely() const override
    {
        if (remotely_ != static_cast<int>(false)
            && remotely_ != static_cast<int>(true))
        {
            throw std::range_error("remotely");
        }
        return static_cast<bool>(remotely_);
    }

    bool
    is_async() const override
    {
        if (is_async_ != static_cast<int>(false)
            && is_async_ != static_cast<int>(true))
        {
            throw std::range_error("is_async");
        }
        return static_cast<bool>(is_async_);
    }

    std::string const&
    domain_name() const override
    {
        throw not_implemented_error("my_generic_context::domain_name()");
    }

    cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) override
    {
        throw not_implemented_error("my_generic_context::schedule_after()");
    }

 private:
    int remotely_;
    int is_async_;
};
static_assert(ValidContext<my_generic_context>);

} // namespace

TEST_CASE("is_uncached", tag)
{
    REQUIRE(is_uncached(caching_level_type::none));
    REQUIRE(!is_uncached(caching_level_type::memory));
    REQUIRE(!is_uncached(caching_level_type::full));
    REQUIRE(!is_uncached(caching_level_type::memory_vb));
    REQUIRE(!is_uncached(caching_level_type::full_vb));
}

TEST_CASE("is_cached", tag)
{
    REQUIRE(!is_cached(caching_level_type::none));
    REQUIRE(is_cached(caching_level_type::memory));
    REQUIRE(is_cached(caching_level_type::full));
    REQUIRE(is_cached(caching_level_type::memory_vb));
    REQUIRE(is_cached(caching_level_type::full_vb));
}

TEST_CASE("is_memory_cached", tag)
{
    REQUIRE(!is_memory_cached(caching_level_type::none));
    REQUIRE(is_memory_cached(caching_level_type::memory));
    REQUIRE(!is_memory_cached(caching_level_type::full));
    REQUIRE(is_memory_cached(caching_level_type::memory_vb));
    REQUIRE(!is_memory_cached(caching_level_type::full_vb));
}

TEST_CASE("is_fully_cached", tag)
{
    REQUIRE(!is_fully_cached(caching_level_type::none));
    REQUIRE(!is_fully_cached(caching_level_type::memory));
    REQUIRE(is_fully_cached(caching_level_type::full));
    REQUIRE(!is_fully_cached(caching_level_type::memory_vb));
    REQUIRE(is_fully_cached(caching_level_type::full_vb));
}

TEST_CASE("is_composition_based", tag)
{
    REQUIRE(!is_composition_based(caching_level_type::none));
    REQUIRE(is_composition_based(caching_level_type::memory));
    REQUIRE(is_composition_based(caching_level_type::full));
    REQUIRE(!is_composition_based(caching_level_type::memory_vb));
    REQUIRE(!is_composition_based(caching_level_type::full_vb));
}

TEST_CASE("is_value_based", tag)
{
    REQUIRE(!is_value_based(caching_level_type::none));
    REQUIRE(!is_value_based(caching_level_type::memory));
    REQUIRE(!is_value_based(caching_level_type::full));
    REQUIRE(is_value_based(caching_level_type::memory_vb));
    REQUIRE(is_value_based(caching_level_type::full_vb));
}

TEST_CASE("to_composition_based", tag)
{
    REQUIRE(
        to_composition_based(caching_level_type::none)
        == caching_level_type::none);
    REQUIRE(
        to_composition_based(caching_level_type::memory)
        == caching_level_type::memory);
    REQUIRE(
        to_composition_based(caching_level_type::full)
        == caching_level_type::full);
    REQUIRE(
        to_composition_based(caching_level_type::memory_vb)
        == caching_level_type::memory);
    REQUIRE(
        to_composition_based(caching_level_type::full_vb)
        == caching_level_type::full);
}

TEST_CASE("convert async_status to string", tag)
{
    REQUIRE(to_string(async_status::CREATED) == "CREATED");
    REQUIRE(to_string(async_status::SUBS_RUNNING) == "SUBS_RUNNING");
    REQUIRE(to_string(async_status::SELF_RUNNING) == "SELF_RUNNING");
    REQUIRE(to_string(async_status::CANCELLED) == "CANCELLED");
    REQUIRE(to_string(async_status::AWAITING_RESULT) == "AWAITING_RESULT");
    REQUIRE(to_string(async_status::FINISHED) == "FINISHED");
    REQUIRE(to_string(async_status::FAILED) == "FAILED");
    REQUIRE(
        to_string(static_cast<async_status>(789)) == "bad async_status 789");
}

TEST_CASE("cast context reference to remote_context_intf", tag)
{
    my_generic_context ctx00{false, undef};
    REQUIRE(cast_ctx_to_ptr<remote_context_intf>(ctx00) == nullptr);
    REQUIRE_THROWS(cast_ctx_to_ref<remote_context_intf>(ctx00));

    my_generic_context ctx01{true, undef};
    REQUIRE(cast_ctx_to_ptr<remote_context_intf>(ctx01) == &ctx01);
    REQUIRE(&cast_ctx_to_ref<remote_context_intf>(ctx01) == &ctx01);

    my_local_only_context ctx02;
    REQUIRE(cast_ctx_to_ptr<remote_context_intf>(ctx02) == nullptr);
    REQUIRE_THROWS(cast_ctx_to_ref<remote_context_intf>(ctx02));

    my_remote_only_context ctx03;
    REQUIRE(cast_ctx_to_ptr<remote_context_intf>(ctx03) == &ctx03);
    REQUIRE(&cast_ctx_to_ref<remote_context_intf>(ctx03) == &ctx03);
}

TEST_CASE("cast context reference to local_context_intf", tag)
{
    my_generic_context ctx10{true, undef};
    REQUIRE(cast_ctx_to_ptr<local_context_intf>(ctx10) == nullptr);
    REQUIRE_THROWS(cast_ctx_to_ref<local_context_intf>(ctx10));

    my_generic_context ctx11{false, undef};
    REQUIRE(cast_ctx_to_ptr<local_context_intf>(ctx11) == &ctx11);
    REQUIRE(&cast_ctx_to_ref<local_context_intf>(ctx11) == &ctx11);

    my_remote_only_context ctx12;
    REQUIRE(cast_ctx_to_ptr<local_context_intf>(ctx12) == nullptr);
    REQUIRE_THROWS(cast_ctx_to_ref<local_context_intf>(ctx12));

    my_local_only_context ctx13;
    REQUIRE(cast_ctx_to_ptr<local_context_intf>(ctx13) == &ctx13);
    REQUIRE(&cast_ctx_to_ref<local_context_intf>(ctx13) == &ctx13);
}

TEST_CASE("cast context reference to sync_context_intf", tag)
{
    my_generic_context ctx20{undef, true};
    REQUIRE(cast_ctx_to_ptr<sync_context_intf>(ctx20) == nullptr);
    REQUIRE_THROWS(cast_ctx_to_ref<sync_context_intf>(ctx20));

    my_generic_context ctx21{undef, false};
    REQUIRE(cast_ctx_to_ptr<sync_context_intf>(ctx21) == &ctx21);
    REQUIRE(&cast_ctx_to_ref<sync_context_intf>(ctx21) == &ctx21);

    my_async_only_context ctx22;
    REQUIRE(cast_ctx_to_ptr<sync_context_intf>(ctx22) == nullptr);
    REQUIRE_THROWS(cast_ctx_to_ref<sync_context_intf>(ctx22));

    my_sync_only_context ctx23;
    REQUIRE(cast_ctx_to_ptr<sync_context_intf>(ctx23) == &ctx23);
    REQUIRE(&cast_ctx_to_ref<sync_context_intf>(ctx23) == &ctx23);
}

TEST_CASE("cast context reference to async_context_intf", tag)
{
    my_generic_context ctx30{undef, false};
    REQUIRE(cast_ctx_to_ptr<async_context_intf>(ctx30) == nullptr);
    REQUIRE_THROWS(cast_ctx_to_ref<async_context_intf>(ctx30));

    my_generic_context ctx31{undef, true};
    REQUIRE(cast_ctx_to_ptr<async_context_intf>(ctx31) == &ctx31);
    REQUIRE(&cast_ctx_to_ref<async_context_intf>(ctx31) == &ctx31);

    my_sync_only_context ctx32;
    REQUIRE(cast_ctx_to_ptr<async_context_intf>(ctx32) == nullptr);
    REQUIRE_THROWS(cast_ctx_to_ref<async_context_intf>(ctx32));

    my_async_only_context ctx33;
    REQUIRE(cast_ctx_to_ptr<async_context_intf>(ctx33) == &ctx33);
    REQUIRE(&cast_ctx_to_ref<async_context_intf>(ctx33) == &ctx33);
}

TEST_CASE("cast context shared_ptr to remote_context_intf", tag)
{
    auto ctx00{std::make_shared<my_generic_context>(false, undef)};
    REQUIRE_THROWS(cast_ctx_to_shared_ptr<remote_context_intf>(ctx00));

    auto ctx01{std::make_shared<my_generic_context>(true, undef)};
    REQUIRE(&*cast_ctx_to_shared_ptr<remote_context_intf>(ctx01) == &*ctx01);

    auto ctx02{std::make_shared<my_local_only_context>()};
    REQUIRE_THROWS(cast_ctx_to_shared_ptr<remote_context_intf>(ctx02));

    auto ctx03{std::make_shared<my_remote_only_context>()};
    REQUIRE(&*cast_ctx_to_shared_ptr<remote_context_intf>(ctx03) == &*ctx03);
}

TEST_CASE("cast context shared_ptr to local_context_intf", tag)
{
    auto ctx10{std::make_shared<my_generic_context>(true, undef)};
    REQUIRE_THROWS(cast_ctx_to_shared_ptr<local_context_intf>(ctx10));

    auto ctx11{std::make_shared<my_generic_context>(false, undef)};
    REQUIRE(&*cast_ctx_to_shared_ptr<local_context_intf>(ctx11) == &*ctx11);

    auto ctx12{std::make_shared<my_remote_only_context>()};
    REQUIRE_THROWS(cast_ctx_to_shared_ptr<local_context_intf>(ctx12));

    auto ctx13{std::make_shared<my_local_only_context>()};
    REQUIRE(&*cast_ctx_to_shared_ptr<local_context_intf>(ctx13) == &*ctx13);
}

TEST_CASE("cast context shared_ptr to sync_context_intf", tag)
{
    auto ctx20{std::make_shared<my_generic_context>(undef, true)};
    REQUIRE_THROWS(cast_ctx_to_shared_ptr<sync_context_intf>(ctx20));

    auto ctx21{std::make_shared<my_generic_context>(undef, false)};
    REQUIRE(&*cast_ctx_to_shared_ptr<sync_context_intf>(ctx21) == &*ctx21);

    auto ctx22{std::make_shared<my_async_only_context>()};
    REQUIRE_THROWS(cast_ctx_to_shared_ptr<sync_context_intf>(ctx22));

    auto ctx23{std::make_shared<my_sync_only_context>()};
    REQUIRE(&*cast_ctx_to_shared_ptr<sync_context_intf>(ctx23) == &*ctx23);
}

TEST_CASE("cast context shared_ptr to async_context_intf", tag)
{
    auto ctx30{std::make_shared<my_generic_context>(undef, false)};
    REQUIRE_THROWS(cast_ctx_to_shared_ptr<async_context_intf>(ctx30));

    auto ctx31{std::make_shared<my_generic_context>(undef, true)};
    REQUIRE(&*cast_ctx_to_shared_ptr<async_context_intf>(ctx31) == &*ctx31);

    auto ctx32{std::make_shared<my_sync_only_context>()};
    REQUIRE_THROWS(cast_ctx_to_shared_ptr<async_context_intf>(ctx32));

    auto ctx33{std::make_shared<my_async_only_context>()};
    REQUIRE(&*cast_ctx_to_shared_ptr<async_context_intf>(ctx33) == &*ctx33);
}
