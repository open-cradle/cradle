#include <cradle/typing/encodings/json.h>
#include <cradle/typing/encodings/msgpack.h>
#include <cradle/typing/io/http_requests.hpp>

namespace cradle {

prep_http_request
make_prep_http_request(http_request const& orig)
{
    return prep_http_request{
        static_cast<prep_http_request_method>(orig.method),
        orig.url,
        orig.headers,
        orig.body,
        orig.socket};
}

prep_http_response
make_prep_http_response(http_response const& orig)
{
    return prep_http_response{orig.status_code, orig.headers, orig.body};
}

dynamic
parse_json_response(http_response const& response)
{
    return parse_json_value(
        reinterpret_cast<char const*>(response.body.data()),
        response.body.size());
}

dynamic
parse_msgpack_response(http_response const& response)
{
    return parse_msgpack_value(
        reinterpret_cast<uint8_t const*>(response.body.data()),
        response.body.size());
}

} // namespace cradle
