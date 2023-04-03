#ifndef CRADLE_INNER_REMOTE_ASYNC_DB_H
#define CRADLE_INNER_REMOTE_ASYNC_DB_H

#include <map>
#include <memory>
#include <mutex>

#include <cradle/inner/requests/generic.h>

namespace cradle {

// Database of local_async_context_intf objects, identified by their id.
// RPC clients specify remote tasks by their async_id value. The server keeps
// an instance of this database so that it can map these values to task-related
// context objects.
// Apart from the (implicit) default ctor and dtor, all functions are
// thread-safe.
class async_db
{
 public:
    // Adds a context object to the database.
    void
    add(std::shared_ptr<local_async_context_intf> ctx);

    // Finds the context object for an async_id value.
    // Throws bad_async_id_error if not found.
    // Returning a shared_ptr ensures that the reference remains valid even
    // with a simultaneous remove_tree() operation.
    std::shared_ptr<local_async_context_intf>
    find(async_id aid);

    // Removes the context objects for the context tree whose root is formed by
    // root_id.
    // Should be called, on client's initiative, when the corresponding
    // request resolution has finished.
    void
    remove_tree(async_id root_id);

 private:
    std::mutex mutex_;
    std::map<async_id, std::shared_ptr<local_async_context_intf>> entries_;

    std::shared_ptr<local_async_context_intf>
    find_no_lock(async_id aid);

    void
    remove_subtree(local_async_context_intf& node_ctx);
};

} // namespace cradle

#endif
