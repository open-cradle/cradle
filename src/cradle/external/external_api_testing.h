// Extra functionality that may be needed by unit tests, but not by normal
// users of the external API.
#ifndef CRADLE_EXTERNAL_API_TESTING_H
#define CRADLE_EXTERNAL_API_TESTING_H

#include <cradle/external_api.h>
#include <cradle/thinknode/service/core.h>

namespace cradle {

namespace external {

cradle::service_core&
get_service_core(api_session& session);

} // namespace external

} // namespace cradle

#endif
