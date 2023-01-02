#ifndef CRADLE_PLUGINS_SERIALIZATION_REQUEST_CEREAL_JSON_H
#define CRADLE_PLUGINS_SERIALIZATION_REQUEST_CEREAL_JSON_H

#include <sstream>
#include <string>

#include <cereal/archives/json.hpp>

namespace cradle {

template<typename Req>
std::string
serialize_request(Req const& req)
{
    std::stringstream os;
    cereal::JSONOutputArchive oarchive(os);
    oarchive(req);
    std::string seri_req = os.str();
    // TODO the serialization _from_ the request has prepended
    // { "value0" :
    // but not appended the corresponding brace.
    return seri_req.substr(16);
}

template<typename Req>
auto
deserialize_request(std::string const& seri_req)
{
    std::istringstream is(seri_req);
    cereal::JSONInputArchive iarchive(is);
    return Req(iarchive);
}

} // namespace cradle

#endif
