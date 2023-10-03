#include <exception>

#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

seri_catalog::~seri_catalog()
{
    unregister_all();
}

void
seri_catalog::unregister_all() noexcept
{
    try
    {
        seri_registry::instance().unregister_catalog(cat_id_);
    }
    catch (std::exception const& e)
    {
        try
        {
            ensure_my_logger();
            logger_->error(
                "seri_catalog::unregister_all() failed: {}", e.what());
        }
        catch (...)
        {
        }
    }
}

void
seri_catalog::ensure_my_logger()
{
    // TODO not thread-safe - should it be?
    if (!logger_)
    {
        logger_ = ensure_logger("cradle");
    }
}

void
selfreg_seri_catalog::register_all()
{
    if (registered_.exchange(true, std::memory_order_relaxed))
    {
        ensure_my_logger();
        logger_->warn("Ignoring spurious seri_catalog::register_all() call");
        return;
    }
    try
    {
        try_register_all();
    }
    catch (...)
    {
        unregister_all();
        throw;
    }
    registered_ = true;
}

} // namespace cradle
