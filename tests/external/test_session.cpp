#include <utility>

#include "test_session.h"
#include <cradle/external/external_api_testing.h>
#include <cradle/external_api.h>
#include <cradle/plugins/disk_cache/storage/local/local_disk_cache_plugin.h>

using namespace cradle;

external_test_session
make_external_test_session()
{
    activate_local_disk_cache_plugin();
    cradle::external::api_service_config service_config{};
    cradle::external::api_service service{
        cradle::external::start_service(service_config)};
    cradle::external::api_thinknode_session_config session_config{
        .api_url = "https://mgh.thinknode.io/api/v1.0",
        .access_token = "xyz",
    };
    auto session = cradle::external::start_session(service, session_config);
    return external_test_session{std::move(service), std::move(session)};
}

external_test_session::external_test_session(
    cradle::external::api_service&& service,
    cradle::external::api_session&& session)
    : service_{std::move(service)}, session_{std::move(session)}
{
}

mock_http_session&
external_test_session::enable_http_mocking()
{
    auto& inner_service = cradle::external::get_service_core(session_);
    init_test_service(inner_service);
    return cradle::enable_http_mocking(inner_service);
}
