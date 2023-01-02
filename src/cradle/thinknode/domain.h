#ifndef CRADLE_THINKNODE_DOMAIN_H
#define CRADLE_THINKNODE_DOMAIN_H

#include <memory>

#include <cradle/inner/requests/domain.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

class thinknode_domain : public domain
{
 public:
    ~thinknode_domain() = default;

    void
    initialize() override;

    void
    set_session(thinknode_session const& session);

    std::string
    name() const override;

    std::shared_ptr<context_intf>
    make_local_context(inner_resources& service) override;

 private:
    thinknode_session session_;

    void
    ensure_session();
};

} // namespace cradle

#endif
