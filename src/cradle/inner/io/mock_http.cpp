#include <cradle/inner/io/mock_http.h>

#include <cradle/inner/utilities/errors.h>

#include <algorithm>

namespace cradle {

mock_http_session::mock_http_session()
    : synchronous_connection_(mock_http_connection(*this))
{
}

mock_http_session::mock_http_session(mock_http_script script)
    : synchronous_connection_(mock_http_connection(*this))
{
    set_script(std::move(script));
}

void
mock_http_session::set_script(mock_http_script script)
{
    script_ = std::move(script);
    in_order_ = true;
}

void
mock_http_session::set_canned_response(http_response const& response)
{
    canned_response_ = std::make_unique<http_response>(response);
    in_order_ = true;
}

bool
mock_http_session::enabled_for(http_request const& request) const
{
    return request.url.find("://localhost") == std::string::npos;
}

bool
mock_http_session::is_complete() const
{
    return script_.empty();
}

bool
mock_http_session::is_in_order() const
{
    return in_order_;
}

http_response
mock_http_connection::perform_request(
    check_in_interface&,
    progress_reporter_interface&,
    http_request const& request)
{
    // These calls may arrive from different threads in the HTTP thread pool.
    std::scoped_lock<std::mutex> lock(session_.mutex_);
    if (session_.canned_response_)
    {
        return *session_.canned_response_;
    }
    auto exchange
        = std::ranges::find_if(session_.script_, [&](auto const& exchange) {
              return exchange.request == request;
          });
    if (exchange == session_.script_.end())
    {
        CRADLE_THROW(
            internal_check_failed()
            << internal_error_message_info("unrecognized mock HTTP request")
            << attempted_http_request_info(request));
    }
    if (exchange != session_.script_.begin())
        session_.in_order_ = false;
    http_response response = exchange->response;
    session_.script_.erase(exchange);
    return response;
}

} // namespace cradle
