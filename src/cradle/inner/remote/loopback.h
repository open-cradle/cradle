#ifndef CRADLE_INNER_REMOTE_LOOPBACK_H
#define CRADLE_INNER_REMOTE_LOOPBACK_H

#include <memory>

#include <BS_thread_pool.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/resolve/seri_lock.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

// Configuration keys for the loopback service
struct loopback_config_keys
{
    // (Optional integer)
    // How many asynchronous root requests can run in parallel,
    // on the loopback service
    inline static std::string const ASYNC_CONCURRENCY{
        "loopback/async_concurrency"};
};

/**
 * The loopback service simulates a remote executor, but acts locally.
 * It still resolves serialized requests into serialized responses.
 */
class loopback_service : public remote_proxy
{
 public:
    loopback_service(std::unique_ptr<inner_resources> resources);

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

    request_essentials
    get_essentials(async_id root_aid) override;

    void
    request_cancellation(async_id aid) override;

    void
    finish_async(async_id root_aid) override;

    tasklet_info_list
    get_tasklet_infos(bool include_finished) override;

    void
    load_shared_library(std::string dir_path, std::string dll_name) override;

    void
    unload_shared_library(std::string dll_name) override;

    void
    mock_http(std::string const& response_body) override;

    void
    clear_unused_mem_cache_entries() override;

    void
    release_cache_record_lock(remote_cache_record_id record_id) override;

    int
    get_num_contained_calls() const override;

 private:
    std::unique_ptr<inner_resources> resources_;
    bool testing_;
    std::shared_ptr<spdlog::logger> logger_;
    BS::thread_pool async_pool_;

    async_db&
    get_async_db();

    seri_cache_record_lock_t
    alloc_cache_record_lock_if_needed(bool need_record_lock);
};

} // namespace cradle

#endif
