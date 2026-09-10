#ifndef CRADLE_INNER_REQUESTS_SERIALIZATION_H
#define CRADLE_INNER_REQUESTS_SERIALIZATION_H

// Requests are always serialized to/from JSON, via cereal.

#include <sstream>
#include <string>

#include <cereal/archives/json.hpp>

namespace cradle {

class inner_resources;

class JSONRequestInputArchive : public cereal::JSONInputArchive
{
 public:
    JSONRequestInputArchive(std::istream& stream, inner_resources& resources)
        : cereal::JSONInputArchive(stream), resources_{resources}
    {
    }

    inner_resources&
    get_resources()
    {
        return resources_;
    }

 private:
    inner_resources& resources_;
};

using JSONRequestOutputArchive = cereal::JSONOutputArchive;

template<typename Req>
std::string
serialize_request(Req const& req)
{
    std::stringstream os;
    {
        JSONRequestOutputArchive oarchive(os);
        // Assumes Req::save() to exist. The canonical way would be
        //   oarchive(req);
        // but this adds a redundant outer "value0".
        req.save(oarchive);
    }
    return os.str();
}

template<typename Req>
auto
deserialize_request(inner_resources& resources, std::string const& seri_req)
{
    std::istringstream is(seri_req);
    JSONRequestInputArchive iarchive(is, resources);
    return Req(iarchive);
}

// Deserializes a request whose arguments were written as uniform embedded
// value blobs (the pool plain-args variant), decoding each argument via
// Req::load_plain_args() rather than by its own cereal type. Used only on the
// pool consume path; the default deserialize_request() is unchanged.
template<typename Req>
Req
deserialize_request_plain_args(
    inner_resources& resources, std::string const& seri_req)
{
    std::istringstream is(seri_req);
    JSONRequestInputArchive iarchive(is, resources);
    Req req;
    req.load_plain_args(iarchive);
    return req;
}

} // namespace cradle

#endif
