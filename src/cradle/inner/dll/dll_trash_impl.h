#ifndef CRADLE_INNER_DLL_DLL_TRASH_IMPL_H
#define CRADLE_INNER_DLL_DLL_TRASH_IMPL_H

#include <vector>

#include <cradle/inner/dll/dll_trash.h>

namespace cradle {

/*
 * Nominal dll_trash implementation
 *
 * The owning object must ensure that the functions are thread-safe.
 */
class dll_trash_impl : public dll_trash
{
 public:
    void
    add(boost::dll::shared_library* lib) override
    {
        libs_.push_back(lib);
    }

    std::size_t
    size() const override
    {
        return libs_.size();
    }

 private:
    std::vector<boost::dll::shared_library*> libs_;
};

} // namespace cradle

#endif
