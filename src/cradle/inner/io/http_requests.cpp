#include <cradle/inner/io/http_requests.h>

#include <cstring>
#include <filesystem>

#include <boost/algorithm/string.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <curl/curl.h>
#include <fmt/format.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/core/monitoring.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/io/http_requests_internal.h>
#include <cradle/inner/utilities/errors.h>

namespace cradle {

char const*
get_value_id(http_request_method value)
{
    switch (value)
    {
        case http_request_method::POST:
            return "POST";
        case http_request_method::GET:
            return "GET";
        case http_request_method::PUT:
            return "PUT";
        case http_request_method::DELETE:
            return "DELETE";
        case http_request_method::PATCH:
            return "PATCH";
        case http_request_method::HEAD:
            return "HEAD";
    }
    CRADLE_THROW(
        cradle::invalid_enum_value() << cradle::enum_id_info(
            "http_request_method") << cradle::enum_value_info(int(value)));
}

std::ostream&
operator<<(std::ostream& s, http_request_method const& x)
{
    s << get_value_id(x);
    return s;
}

static std::string
formatted(std::map<std::string, std::string> const& x)
{
    std::ostringstream os;
    os << "{";
    bool need_sep = false;
    for (auto const& kv : x)
    {
        if (need_sep)
        {
            os << ", ";
        }
        os << fmt::format("{}: {}", kv.first, kv.second);
        need_sep = true;
    }
    os << "}";
    return os.str();
}

static std::string
formatted(std::optional<std::string> const& x)
{
    if (x)
    {
        return *x;
    }
    return "(none)";
}

bool
operator==(http_request const& a, http_request const& b)
{
    return a.method == b.method && a.url == b.url && a.headers == b.headers
           && a.body == b.body && a.socket == b.socket;
}

bool
operator!=(http_request const& a, http_request const& b)
{
    return !(a == b);
}

std::ostream&
operator<<(std::ostream& s, http_request const& x)
{
    s << fmt::format(
        "http_request(method={}, url={}, headers={}, body={}, socket={})",
        get_value_id(x.method),
        x.url,
        formatted(x.headers),
        x.body,
        formatted(x.socket));
    return s;
}

bool
operator==(http_response const& a, http_response const& b)
{
    return a.status_code == b.status_code && a.headers == b.headers
           && a.body == b.body;
}

bool
operator!=(http_response const& a, http_response const& b)
{
    return !(a == b);
}

std::ostream&
operator<<(std::ostream& s, http_response const& x)
{
    s << fmt::format(
        "http_reponse(status_code={}, headers={}, body={})",
        x.status_code,
        formatted(x.headers),
        x.body);
    return s;
}

http_response
make_http_200_response(std::string body)
{
    http_response response;
    response.status_code = 200;
    response.body = make_blob(std::move(body));
    return response;
}

http_request_system::http_request_system()
{
    if (curl_global_init(CURL_GLOBAL_ALL))
    {
        CRADLE_THROW(http_request_system_error());
    }
}
http_request_system::~http_request_system()
{
    curl_global_cleanup();
}

struct http_connection_impl
{
    CURL* curl;
};

static void
reset_curl_connection(http_connection_impl& connection)
{
    CURL* curl = connection.curl;
    curl_easy_reset(curl);

    // Allow requests to be redirected.
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Tell CURL to accept and decode gzipped responses.
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 1);

    // Enable SSL verification.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);

    // https://curl.se/libcurl/c/threadsafe.html says:
    // When using multiple threads you should set the CURLOPT_NOSIGNAL option
    // to 1L for all handles.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
}

http_connection::http_connection(http_request_system& system)
{
    impl_.reset(new http_connection_impl);

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        CRADLE_THROW(http_request_system_error());
    }
    impl_->curl = curl;
}

http_connection::~http_connection()
{
    if (impl_)
        curl_easy_cleanup(impl_->curl);
}

http_connection::http_connection(http_connection&&) = default;
http_connection&
http_connection::operator=(http_connection&&)
    = default;

struct send_transmission_state
{
    std::byte const* data = nullptr;
    size_t data_length = 0;
    size_t read_position = 0;
};

static size_t
transmit_request_body(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    send_transmission_state& state
        = *reinterpret_cast<send_transmission_state*>(userdata);
    size_t n_bytes
        = (std::min)(size * nmemb, state.data_length - state.read_position);
    if (n_bytes > 0)
    {
        assert(state.data);
        std::memcpy(ptr, state.data + state.read_position, n_bytes);
        state.read_position += n_bytes;
    }
    return n_bytes;
}

struct receive_transmission_state
{
    malloc_buffer_ptr buffer;
    size_t buffer_length = 0;
    size_t write_position = 0;

    receive_transmission_state() : buffer(nullptr, free)
    {
    }
};

static size_t
record_http_response(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    receive_transmission_state& state
        = *reinterpret_cast<receive_transmission_state*>(userdata);
    if (!state.buffer)
    {
        char* allocation = reinterpret_cast<char*>(malloc(4096));
        if (!allocation)
            return 0;
        state.buffer = malloc_buffer_ptr(allocation, free);
        state.buffer_length = 4096;
        state.write_position = 0;
    }

    // Grow the buffer if necessary.
    size_t n_bytes = size * nmemb;
    if (state.buffer_length < (state.write_position + n_bytes))
    {
        // Each time the buffer grows, it doubles in size.
        // This wastes some memory but should be faster in general.
        size_t new_size = state.buffer_length * 2;
        while (new_size < state.buffer_length + n_bytes)
            new_size *= 2;
        char* allocation = reinterpret_cast<char*>(
            realloc(state.buffer.release(), new_size));
        if (!allocation)
            return 0;
        state.buffer = malloc_buffer_ptr(allocation, free);
        state.buffer_length = new_size;
    }

    assert(state.buffer);
    std::memcpy(state.buffer.get() + state.write_position, ptr, n_bytes);
    state.write_position += n_bytes;
    return n_bytes;
}

static void
set_up_send_transmission(
    CURL* curl,
    send_transmission_state& send_state,
    http_request const& request)
{
    send_state.data = request.body.data();
    send_state.data_length = request.body.size();
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, transmit_request_body);
    curl_easy_setopt(curl, CURLOPT_READDATA, &send_state);
}

struct curl_progress_data
{
    check_in_interface* check_in;
    progress_reporter_interface* reporter;
};

static int
curl_xfer_callback(
    void* clientp,
    curl_off_t dltotal,
    curl_off_t dlnow,
    curl_off_t ultotal,
    curl_off_t ulnow)
{
    curl_progress_data* data = reinterpret_cast<curl_progress_data*>(clientp);
    try
    {
        (*data->check_in)();
        (*data->reporter)(
            (dltotal + ultotal == 0)
                ? 0.f
                : float(double(dlnow + ulnow) / double(dltotal + ultotal)));
    }
    catch (...)
    {
        return 1;
    }
    return 0;
}

struct scoped_curl_slist
{
    ~scoped_curl_slist()
    {
        curl_slist_free_all(list);
    }
    curl_slist* list;
};

static blob
make_blob(receive_transmission_state&& transmission)
{
    auto data{as_bytes(transmission.buffer.get())};
    size_t size{transmission.write_position};
    return blob{
        std::make_shared<malloc_buffer_ptr_wrapper>(
            std::move(transmission.buffer)),
        data,
        size};
}

http_request
redact_request(http_request request)
{
    auto authorization_header = request.headers.find("Authorization");
    if (authorization_header != request.headers.end())
        authorization_header->second = "[redacted]";
    return request;
}

http_response
http_connection::perform_request(
    check_in_interface& check_in,
    progress_reporter_interface& reporter,
    http_request const& request)
{
    auto logger = spdlog::get("cradle");
    logger->info("HTTP perform_request");
    logger->debug("<<< query");
    logger->debug("{}", redact_request(request));
    logger->debug(">>> query");

    CURL* curl = impl_->curl;
    assert(curl);
    reset_curl_connection(*impl_);

    // Set the headers for the request.
    scoped_curl_slist curl_headers;
    curl_headers.list = NULL;
    for (auto const& header : request.headers)
    {
        auto header_string = header.first + ":" + header.second;
        curl_headers.list
            = curl_slist_append(curl_headers.list, header_string.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers.list);

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    if (request.socket)
    {
        curl_easy_setopt(
            curl, CURLOPT_UNIX_SOCKET_PATH, request.socket->c_str());
    }

    // Set up for receiving the response body.
    receive_transmission_state body_receive_state;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, record_http_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body_receive_state);

    // Set up for receiving the response headers.
    receive_transmission_state header_receive_state;
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, record_http_response);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_receive_state);

    // Let CURL know what the method is and set up for sending the body if
    // necessary.
    send_transmission_state send_state;
    switch (request.method)
    {
        case http_request_method::PUT:
            set_up_send_transmission(curl, send_state, request);
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
            curl_easy_setopt(
                curl,
                CURLOPT_INFILESIZE_LARGE,
                curl_off_t(request.body.size()));
            break;

        case http_request_method::PATCH:
            // PATCH uses the same mechanism for sending content as POST but
            // uses a custom request type.
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        case http_request_method::POST:
            set_up_send_transmission(curl, send_state, request);
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(
                curl,
                CURLOPT_POSTFIELDSIZE_LARGE,
                curl_off_t(request.body.size()));
            break;

        case http_request_method::DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;

        case http_request_method::HEAD:
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
            break;

        case http_request_method::GET:
            // This is the default method for Curl.
            break;
    }

    // Set up progress monitoring.
    curl_progress_data progress_data;
    progress_data.check_in = &check_in;
    progress_data.reporter = &reporter;
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_xfer_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);

    // Perform the request.
    CURLcode result = curl_easy_perform(curl);

    // Check in again here because if the job was canceled inside the above
    // call, it will just look like an error. We need the cancellation
    // exception to be rethrown.
    check_in();

    // Check for low-level CURL errors.
    if (result != CURLE_OK)
    {
        CRADLE_THROW(
            http_request_failure()
            << attempted_http_request_info(redact_request(request))
            << internal_error_message_info(curl_easy_strerror(result)));
    }

    // Parse the response headers.
    http_header_list response_headers;
    {
        std::istringstream response_header_text(std::string(
            header_receive_state.buffer.get(),
            header_receive_state.buffer_length));
        std::string header_line;
        while (std::getline(response_header_text, header_line)
               && header_line != "\r")
        {
            auto index = header_line.find(':', 0);
            if (index != std::string::npos)
            {
                response_headers[boost::algorithm::trim_copy(
                    header_line.substr(0, index))]
                    = boost::algorithm::trim_copy(
                        header_line.substr(index + 1));
            }
        }
    }

    // Construct the response.
    http_response response;
    response.body = make_blob(std::move(body_receive_state));
    response.headers = std::move(response_headers);
    long status_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    response.status_code = boost::numeric_cast<int>(status_code);

    // Check the status code.
    if (status_code < 200 || status_code > 299)
    {
        CRADLE_THROW(
            bad_http_status_code()
            << attempted_http_request_info(redact_request(request))
            << http_response_info(response));
    }

    logger->debug("<<< response");
    logger->debug("{}", response);
    logger->debug(">>> response");

    return response;
}

} // namespace cradle
