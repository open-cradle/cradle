#include <cradle/plugins/domain/testing/domain.h>
#include <cradle/plugins/domain/testing/domain_factory.h>

namespace cradle {

std::unique_ptr<domain>
create_testing_domain(inner_resources& resources)
{
    return std::make_unique<testing_domain>(resources);
}

} // namespace cradle
