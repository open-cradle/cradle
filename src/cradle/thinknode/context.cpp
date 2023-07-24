#include <cradle/inner/remote/config.h>
#include <cradle/thinknode/config.h>
#include <cradle/thinknode/context.h>

namespace cradle {

namespace {

std::string const the_domain_name{"thinknode"};

thinknode_session
make_session(service_config const& config)
{
    // Running on a remote server. Client must pass config including api_url
    // and access_token.
    return thinknode_session{
        config.get_mandatory_string(thinknode_config_keys::API_URL),
        config.get_mandatory_string(thinknode_config_keys::ACCESS_TOKEN)};
}

} // namespace

thinknode_request_context::thinknode_request_context(
    service_core& service, service_config const& config)
    : sync_context_base{service, nullptr, false, ""},
      service{service},
      session{make_session(config)}
{
}

thinknode_request_context::thinknode_request_context(
    service_core& service,
    thinknode_session session,
    tasklet_tracker* tasklet,
    bool remotely,
    std::string proxy_name)
    : sync_context_base{service, tasklet, remotely, std::move(proxy_name)},
      service{service},
      session{std::move(session)}
{
}

thinknode_request_context::~thinknode_request_context()
{
}

std::string const&
thinknode_request_context::domain_name() const
{
    return the_domain_name;
}

service_config
thinknode_request_context::make_config() const
{
    service_config_map config_map{
        {remote_config_keys::DOMAIN_NAME, domain_name()},
        {thinknode_config_keys::API_URL, session.api_url},
        {thinknode_config_keys::ACCESS_TOKEN, session.access_token},
    };
    return service_config{config_map};
}

} // namespace cradle
