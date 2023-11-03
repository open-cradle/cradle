#include <cradle/thinknode/domain.h>
#include <cradle/thinknode/domain_factory.h>

namespace cradle {

std::unique_ptr<domain>
create_thinknode_domain(service_core& resources)
{
    return std::make_unique<thinknode_domain>(resources);
}

} // namespace cradle
