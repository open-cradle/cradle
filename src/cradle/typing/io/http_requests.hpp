#ifndef CRADLE_TYPING_IO_HTTP_REQUESTS_HPP
#define CRADLE_TYPING_IO_HTTP_REQUESTS_HPP

#include <cradle/inner/io/http_requests.h>
#include <cradle/typing/core/type_definitions.h>

namespace cradle {

// supported HTTP request methods
// Like enum class http_request_method, but subject to preprocessing
api(enum internal)
enum class prep_http_request_method
{
    POST,
    GET,
    PUT,
    DELETE,
    PATCH,
    HEAD
};

// Like struct prep_http_request, but subject to preprocessing
api(struct internal)
struct prep_http_request
{
    prep_http_request_method method;
    string url;
    http_header_list headers;
    blob body;
    optional<string> socket;
};

prep_http_request
make_prep_http_request(http_request const& orig);

// Like struct prep_http_response, but subject to preprocessing
api(struct internal)
struct prep_http_response
{
    int status_code;
    http_header_list headers;
    blob body;
};

prep_http_response
make_prep_http_response(http_response const& orig);

// Parse a http_response as a JSON value.
dynamic
parse_json_response(http_response const& response);

// Parse a http_response as a MessagePack value.
dynamic
parse_msgpack_response(http_response const& response);

} // namespace cradle

#endif
