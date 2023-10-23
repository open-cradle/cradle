#include <exception>

#include <spdlog/spdlog.h>

#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

seri_catalog::seri_catalog(std::shared_ptr<seri_registry> registry)
    : registry_{std::move(registry)}
{
}

seri_catalog::~seri_catalog()
{
    unregister_all();
}

void
seri_catalog::unregister_all() noexcept
{
    try
    {
        registry_->unregister_catalog(cat_id_);
    }
    catch (std::exception const& e)
    {
        try
        {
            auto logger{ensure_logger("cradle")};
            logger->error(
                "seri_catalog::unregister_all() failed: {}", e.what());
        }
        catch (...)
        {
        }
    }
}

selfreg_seri_catalog::selfreg_seri_catalog(
    std::shared_ptr<seri_registry> registry)
    : seri_catalog{std::move(registry)}
{
}

} // namespace cradle
