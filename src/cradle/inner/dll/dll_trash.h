#ifndef CRADLE_INNER_DLL_DLL_TRASH_H
#define CRADLE_INNER_DLL_DLL_TRASH_H

#include <cstddef>

#include <boost/dll.hpp>

namespace cradle {

/*
 * Container of DLLs that are no longer active, but not actually unloaded
 * (see comments for class dll_controller).
 * Maybe useful for reporting/debugging purposes.
 *
 * An implementation of this class must ensure that the functions are
 * thread-safe.
 */
class dll_trash
{
 public:
    virtual ~dll_trash() = default;

    virtual void
    add(boost::dll::shared_library* lib)
        = 0;

    virtual std::size_t
    size() const
        = 0;
};

} // namespace cradle

#endif
