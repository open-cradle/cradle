#ifndef CRADLE_INNER_REMOTE_LOOPBACK_IMPL_H
#define CRADLE_INNER_REMOTE_LOOPBACK_IMPL_H

#include <memory>

#include <BS_thread_pool.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

class loopback_service : public remote_proxy
{
 public:
    loopback_service(service_config const& config, inner_resources& resources);

    std::string
    name() const override;

    spdlog::logger&
    get_logger() override;

    // If true, resolve_async() has an artifical delay at the start.
    // For testing purposes only.
    bool
    delayed_resolve_async() const
    {
        return delayed_resolve_async_;
    }

    serialized_result
    resolve_sync(
        remote_context_intf& ctx,
        std::string domain_name,
        std::string seri_req) override;

    async_id
    submit_async(
        remote_context_intf& ctx,
        std::string domain_name,
        std::string seri_req) override;

    remote_context_spec_list
    get_sub_contexts(async_id aid) override;

    async_status
    get_async_status(async_id aid) override;

    std::string
    get_async_error_message(async_id aid) override;

    serialized_result
    get_async_response(async_id root_aid) override;

    void
    request_cancellation(async_id aid) override;

    void
    finish_async(async_id root_aid) override;

 private:
    inner_resources& resources_;
    bool testing_;
    bool delayed_resolve_async_{false};
    inline static std::shared_ptr<spdlog::logger> logger_;
    BS::thread_pool async_pool_;

    async_db&
    get_async_db();
};

} // namespace cradle

#endif
