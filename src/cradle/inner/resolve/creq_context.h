#ifndef CRADLE_INNER_RESOLVE_CREQ_CONTEXT_H
#define CRADLE_INNER_RESOLVE_CREQ_CONTEXT_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include <spdlog/spdlog.h>

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/test_context.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

class inner_resources;

// Context used for resolving a function request in a contained process.
// Need to detect when the contained process has crashed. This is accomplished
// by having timeouts on the RPC calls to the process, meaning the context
// must be async, even if the client provides a sync one.
class creq_context : public remote_async_context_intf,
                     public test_params_context_mixin
{
 public:
    creq_context(
        inner_resources& resources,
        std::shared_ptr<spdlog::logger> logger,
        std::string domain_name);

    ~creq_context();

    // Once created, these objects should not be moved.
    creq_context(creq_context const&) = delete;
    void
    operator=(creq_context const&)
        = delete;
    creq_context(creq_context&&) = delete;
    void
    operator=(creq_context&&)
        = delete;

    // To be called after the contained function call succeeded; only then will
    // the proxy process be kept alive.
    void
    mark_succeeded();

    // context_intf
    inner_resources&
    get_resources() override
    {
        return resources_;
    }

    bool
    remotely() const override
    {
        return true;
    }

    bool
    is_async() const override
    {
        return true;
    }

    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

    cppcoro::task<>
    schedule_after(std::chrono::milliseconds delay) override;

    // remote_context_intf
    remote_proxy&
    get_proxy() const override
    {
        return *proxy_;
    }

    service_config
    make_config(bool need_record_lock) const override;

    // async_context_intf
    // Most functions in this interface won't be called.
    async_id
    get_id() const override;

    bool
    is_req() const override;

    std::size_t
    get_num_subs() const override;

    async_context_intf&
    get_sub(std::size_t ix) override;

    cppcoro::task<async_status>
    get_status_coro() override;

    cppcoro::task<void>
    request_cancellation_coro() override;

    // remote_async_context_intf
    void
    set_remote_id(async_id remote_id) override;

    void
    fail_remote_id() noexcept override;

    async_id
    get_remote_id() override
    {
        return remote_id_;
    }

    void
    make_introspective() override;

    bool
    introspective() const override;

    // special
    void
    throw_if_cancelled();

 private:
    inner_resources& resources_;
    std::shared_ptr<spdlog::logger> logger_;
    std::string const domain_name_;
    std::string const proxy_name_{"creq"};
    std::unique_ptr<rpclib_client> proxy_;

    // Unless succeeded_ was set to true, this object's destructor terminates
    // the proxy process.
    std::atomic<bool> succeeded_{false};

    // The mutex prevents race conditions between request_cancellation_coro()
    // and set_remote_id(), ensuring a cancellation request always makes it to
    // the proxy when it is / becomes reachable.
    std::mutex mutex_;
    std::atomic<bool> cancelled_{false};
    std::atomic<async_id> remote_id_{NO_ASYNC_ID};

    void
    finish_remote() noexcept;
};

} // namespace cradle

#endif
