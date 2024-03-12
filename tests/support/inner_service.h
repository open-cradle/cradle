#ifndef CRADLE_TESTS_SUPPORT_INNER_SERVICE_H
#define CRADLE_TESTS_SUPPORT_INNER_SERVICE_H

#include <memory>

#include "common.h"
#include <cradle/inner/core/exception.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/resources.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

service_config
make_inner_tests_config();

std::unique_ptr<inner_resources>
make_inner_test_resources(
    std::string const& proxy_name = {},
    domain_option const& domain = no_domain_option());

class non_caching_request_resolution_context final : public local_context_intf,
                                                     public sync_context_intf
{
 public:
    // TODO these resources should not have caches
    non_caching_request_resolution_context(inner_resources& resources);

    // context_intf
    inner_resources&
    get_resources() override
    {
        return resources_;
    }

    bool
    remotely() const override
    {
        return false;
    }

    bool
    is_async() const override
    {
        return false;
    }

    // local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) override
    {
        throw not_implemented_error();
    }

    void
    on_value_complete() override
    {
        throw not_implemented_error();
    }

 private:
    inner_resources& resources_;
};

static_assert(ValidContext<non_caching_request_resolution_context>);

class caching_request_resolution_context final : public local_context_intf,
                                                 public sync_context_intf,
                                                 public caching_context_intf
{
 public:
    caching_request_resolution_context(inner_resources& resources);

    // context_intf
    inner_resources&
    get_resources() override
    {
        return resources_;
    }

    bool
    remotely() const override
    {
        return false;
    }

    bool
    is_async() const override
    {
        return false;
    }

    // local_context_intf
    std::shared_ptr<data_owner>
    make_data_owner(std::size_t size, bool use_shared_memory) override
    {
        throw not_implemented_error();
    }

    void
    on_value_complete() override
    {
        throw not_implemented_error();
    }

 private:
    inner_resources& resources_;
};

static_assert(ValidContext<caching_request_resolution_context>);

} // namespace cradle

#endif
