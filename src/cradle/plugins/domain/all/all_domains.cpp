#include <cradle/plugins/domain/all/all_domains.h>
#include <cradle/plugins/domain/testing/domain.h>
#include <cradle/thinknode/domain.h>

namespace cradle {

void
register_and_initialize_all_domains()
{
    register_and_initialize_testing_domain();
    register_and_initialize_thinknode_domain();
}

} // namespace cradle
