#include <memory>
#include <vector>

#include <cradle/plugins/domain/all/all_domains.h>
#include <cradle/plugins/domain/testing/domain.h>
#include <cradle/thinknode/domain.h>

namespace cradle {

void
register_and_initialize_all_domains()
{
    std::vector<std::shared_ptr<domain>> domains{
        std::make_shared<thinknode_domain>(),
        std::make_shared<testing_domain>(),
        std::make_shared<atst_domain>()};
    for (auto& domain : domains)
    {
        register_domain(domain);
        domain->initialize();
    }
}

} // namespace cradle
