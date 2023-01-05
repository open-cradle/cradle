#ifndef CRADLE_INNER_REMOTE_LOOPBACK_IMPL_H
#define CRADLE_INNER_REMOTE_LOOPBACK_IMPL_H

#include <memory>

#include <spdlog/spdlog.h>

#include <cradle/inner/remote/proxy.h>

namespace cradle {

class loopback_service : public remote_proxy
{
 public:
    loopback_service();

    std::string
    name() const override;

    cppcoro::task<blob>
    resolve_request(remote_context_intf& ctx, std::string seri_req) override;

 private:
    inline static std::shared_ptr<spdlog::logger> logger_;
};

} // namespace cradle

#endif
