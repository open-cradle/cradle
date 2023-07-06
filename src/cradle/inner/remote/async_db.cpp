#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/remote/async_db.h>
#include <cradle/inner/remote/async_exceptions.h>

namespace cradle {

void
async_db::add(std::shared_ptr<local_async_context_intf> ctx)
{
    std::scoped_lock lock{mutex_};
    auto aid = ctx->get_id();
    entries_.emplace(aid, std::move(ctx));
}

std::shared_ptr<local_async_context_intf>
async_db::find(async_id aid)
{
    std::scoped_lock lock{mutex_};
    return find_no_lock(aid);
}

std::shared_ptr<local_async_context_intf>
async_db::find_no_lock(async_id aid)
{
    auto it = entries_.find(aid);
    if (it == entries_.end())
    {
        throw bad_async_id_error{fmt::format("unknown async_id {}", aid)};
    }
    return it->second;
}

// Traverses the tree, recursively removing a node's children, then the node
// itself; the root node is removed last.
void
async_db::remove_tree(async_id root_aid)
{
    std::scoped_lock lock{mutex_};
    auto size_before = entries_.size();
    remove_subtree(*find_no_lock(root_aid));
    auto size_after = entries_.size();
    auto logger = spdlog::get("cradle");
    logger->debug(
        "async_db::remove_tree({}) removed {} entries, {} remaining",
        root_aid,
        size_before - size_after,
        size_after);
}

void
async_db::remove_subtree(local_async_context_intf& node_ctx)
{
    auto nsubs = node_ctx.get_local_num_subs();
    for (decltype(nsubs) i = 0; i < nsubs; ++i)
    {
        remove_subtree(node_ctx.get_local_sub(i));
    }
    entries_.erase(node_ctx.get_id());
}

} // namespace cradle
