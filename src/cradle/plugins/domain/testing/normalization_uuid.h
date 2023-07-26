#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_NORMALIZATION_UUID_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_NORMALIZATION_UUID_H

#include <cradle/inner/requests/normalization_uuid.h>

#include <string>

namespace cradle {

// Extra normalization_uuid_str specializations over those in
// inner/requests/normalization_uuid.h.

// E.g.
#if 0
template<>
struct normalization_uuid_str<unsigned>
{
    static const inline std::string func{"normalization<unsigned,func>"};
    static const inline std::string coro{"normalization<unsigned,coro>"};
};
#endif

} // namespace cradle

#endif
