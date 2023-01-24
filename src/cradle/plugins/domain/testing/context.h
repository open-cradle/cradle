#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_CONTEXT_H

#include <memory>
#include <vector>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/requests/generic.h>

namespace cradle {

struct testing_request_context : public cached_introspective_context_intf,
                                 public remote_context_intf
{
    inner_resources& service;

    testing_request_context(
        inner_resources& service,
        tasklet_tracker* tasklet,
        bool remotely = false);

    inner_resources&
    get_resources() override;

    immutable_cache&
    get_cache() override;

    tasklet_tracker*
    get_tasklet() override;

    void
    push_tasklet(tasklet_tracker* tasklet) override;

    void
    pop_tasklet() override;

    bool
    remotely() const override
    {
        return remotely_;
    }

    void
    proxy_name(std::string const& name);

    std::string const&
    proxy_name() const override;

    std::string const&
    domain_name() const override
    {
        return domain_name_;
    }

    std::unique_ptr<remote_context_intf>
    local_clone() const override;

    std::shared_ptr<blob_file_writer>
    make_blob_file_writer(size_t size);

 private:
    std::vector<tasklet_tracker*> tasklets_;
    bool remotely_;
    std::string proxy_name_;
    std::string domain_name_{"testing"};
};

} // namespace cradle

#endif
