#include <unordered_map>

#include <cradle/inner/requests/domain.h>

namespace cradle {

class domain_registry
{
 public:
    static domain_registry&
    instance();

    void
    do_register(std::shared_ptr<domain> const& dom);

    std::shared_ptr<domain>
    find(std::string const& name);

 private:
    std::unordered_map<std::string, std::shared_ptr<domain>> domains_;
};

domain_registry&
domain_registry::instance()
{
    static domain_registry the_instance;
    return the_instance;
}

void
domain_registry::do_register(std::shared_ptr<domain> const& dom)
{
    domains_[dom->name()] = dom;
}

std::shared_ptr<domain>
domain_registry::find(std::string const& name)
{
    return domains_[name];
}

void
register_domain(std::shared_ptr<domain> const& dom)
{
    domain_registry::instance().do_register(dom);
}

std::shared_ptr<domain>
find_domain(std::string const& name)
{
    return domain_registry::instance().find(name);
}

} // namespace cradle
