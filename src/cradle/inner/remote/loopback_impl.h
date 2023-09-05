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

    serialized_result
    resolve_sync(service_config config, std::string seri_req) override;

    async_id
    submit_async(service_config config, std::string seri_req) override;

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

    tasklet_info_tuple_list
    get_tasklet_infos(bool include_finished) override;

 private:
    inner_resources& resources_;
    bool testing_;
    inline static std::shared_ptr<spdlog::logger> logger_;
    BS::thread_pool async_pool_;

    async_db&
    get_async_db();
};

} // namespace cradle

#endif
