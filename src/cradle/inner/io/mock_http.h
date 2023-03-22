#ifndef CRADLE_INNER_IO_MOCK_HTTP_H
#define CRADLE_INNER_IO_MOCK_HTTP_H

#include <memory>
#include <mutex>

#include <cradle/inner/io/http_requests.h>

namespace cradle {

struct mock_http_exchange
{
    http_request request;
    http_response response;
};

typedef std::vector<mock_http_exchange> mock_http_script;

struct mock_http_session;

struct mock_http_connection : http_connection_interface
{
    mock_http_connection(mock_http_session& session) : session_(session)
    {
    }

    http_response
    perform_request(
        check_in_interface& check_in,
        progress_reporter_interface& reporter,
        http_request const& request) override;

 private:
    mock_http_session& session_;
};

struct mock_http_session
{
    mock_http_session();

    mock_http_session(mock_http_script script);

    // Set the script of expected exchanges for this mock HTTP session.
    void
    set_script(mock_http_script script);

    // Set a response that will be returned on each request
    void
    set_canned_response(http_response const& response);

    // Returns true if mocking is enabled for the specified request.
    // Mocking is always disabled for requests to a local server
    // (e.g., for HTTP-based caching).
    bool
    enabled_for(http_request const& request) const;

    // Have all exchanges in the script been executed?
    bool
    is_complete() const;

    // Has the script been executed in order so far?
    bool
    is_in_order() const;

    // Returns a connection that can be used for synchronous HTTP requests.
    // Should be used for benchmark tests (only).
    mock_http_connection&
    synchronous_connection()
    {
        return synchronous_connection_;
    }

 private:
    friend struct mock_http_connection;

    std::mutex mutex_;
    mock_http_script script_;
    std::unique_ptr<http_response> canned_response_;
    mock_http_connection synchronous_connection_;

    // Has the script been executed in order so far?
    bool in_order_ = true;
};

} // namespace cradle

#endif
