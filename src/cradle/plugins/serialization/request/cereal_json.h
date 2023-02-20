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
    {
        cereal::JSONOutputArchive oarchive(os);
        // Assumes Req::save() to exist. The canonical way would be
        //   oarchive(req);
        // but this adds a redundant outer "value0".
        req.save(oarchive);
    }
    return os.str();
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
