#include <vector>

#include <fmt/format.h>

#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/inner/utilities/logging.h>

namespace cradle {

seri_registry&
seri_registry::instance()
{
    // The singleton is part of the main program, and any dynamically loaded
    // library will see this instance.
    static seri_registry the_instance;
    return the_instance;
}

seri_registry::seri_registry() : logger_{ensure_logger("cfr")}
{
}

void
seri_registry::add(
    catalog_id cat_id,
    std::string const& uuid_str,
    create_t* create,
    std::any function)
{
    // TODO investigate exception opportunities and consequences.
    logger_->debug("add uuid {}, cat_id {}", uuid_str, cat_id.value());
    std::scoped_lock lock{mutex_};
    auto outer_it = entries_.find(uuid_str);
    if (outer_it == entries_.end())
    {
        outer_it = entries_.emplace(uuid_str, inner_list_t{}).first;
    }
    auto& inner_list = outer_it->second;
    if (detect_duplicate(inner_list, cat_id, uuid_str))
    {
        return;
    }
    // Any existing matching entry could contain stale pointers, and attempts
    // to overwrite it could lead to crashes. Push new entry to the front so
    // that find_entry() will find it and not a stale one.
    // TODO multiple normalized_arg entries possible?
    inner_list.push_front(entry_t{cat_id, create, std::move(function)});
}

void
seri_registry::unregister_catalog(catalog_id cat_id)
{
    logger_->info("seri_registry: unregister_catalog {}", cat_id.value());
    std::scoped_lock lock{mutex_};
    std::vector<std::string> keys_to_remove;
    for (auto& [uuid_str, inner_list] : entries_)
    {
        for (auto inner_it = inner_list.begin(); inner_it != inner_list.end();)
        {
            if (inner_it->cat_id == cat_id)
            {
                logger_->debug(
                    "removing entry for uuid {}, cat_id {}",
                    uuid_str,
                    cat_id.value());
                inner_it = inner_list.erase(inner_it);
            }
            else
            {
                ++inner_it;
            }
        }
        if (inner_list.empty())
        {
            // Calling erase() here would invalidate the iterator.
            keys_to_remove.push_back(uuid_str);
        }
    }
    for (auto const& key : keys_to_remove)
    {
        logger_->debug("removing empty inner list for uuid {}", key);
        entries_.erase(key);
    }
    log_all_entries(fmt::format("after unload cat_id {}", cat_id.value()));
}

// Finds _an_ entry for uuid_str.
// Assuming that the ODR holds across DLLs, `create` and `function` functions
// implemented in DLL X should be identical to ones implemented in DLL Y.
// TODO keep track of pointers to DLL code and do not unload if they exist
seri_registry::entry_t&
seri_registry::find_entry(std::string const& uuid_str)
{
    std::scoped_lock lock{mutex_};
    auto it = entries_.find(uuid_str);
    if (it == entries_.end())
    {
        throw unregistered_uuid_error(fmt::format(
            "seri_registry: no entry found for uuid {}", uuid_str));
    }
    auto& inner_list = it->second;
    if (inner_list.empty())
    {
        // Violating the invariant that inner_list is not empty.
        throw unregistered_uuid_error(
            fmt::format("seri_registry: empty list for uuid {}", uuid_str));
    }
    // Any entry from inner_list should do.
    return *inner_list.begin();
}

void
seri_registry::log_all_entries(std::string const& when)
{
    if (!logger_->should_log(spdlog::level::debug))
    {
        return;
    }
    logger_->debug("seri_registry has {} entries {}", entries_.size(), when);
    int outer_ix = 0;
    for (auto const& [uuid_str, inner_list] : entries_)
    {
        std::string cat_ids;
        int inner_ix = 0;
        for (auto const& inner_it : inner_list)
        {
            if (inner_ix > 0)
            {
                cat_ids += ",";
            }
            cat_ids += fmt::format(" {}", inner_it.cat_id.value());
            ++inner_ix;
        }
        logger_->debug("({}) uuid {}: cat_id{}", outer_ix, uuid_str, cat_ids);
        ++outer_ix;
    }
}

// Duplicate uuids within a catalog are OK (and common) for normalizers.
// For other uuids, this should not happen.
bool
seri_registry::detect_duplicate(
    inner_list_t const& inner_list,
    catalog_id cat_id,
    std::string const& uuid_str)
{
    // TODO caller should know the is_normalizer value:
    // top-most requests are false, child requests are true.
    bool is_normalizer{uuid_str.starts_with("normalization<")};
    bool is_duplicate{false};
    for (auto& inner_it : inner_list)
    {
        if (inner_it.cat_id == cat_id)
        {
            if (is_normalizer)
            {
                logger_->debug(
                    "duplicate normalizer for uuid {} and cat_id {}",
                    uuid_str,
                    cat_id.value());
            }
            else
            {
                logger_->error(
                    "duplicate entry for uuid {} and cat_id {}",
                    uuid_str,
                    cat_id.value());
            }
            is_duplicate = true;
        }
    }
    return is_duplicate;
}

} // namespace cradle
