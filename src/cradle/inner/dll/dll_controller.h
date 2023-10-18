#ifndef CRADLE_INNER_DLL_DLL_CONTROLLER_H
#define CRADLE_INNER_DLL_DLL_CONTROLLER_H

#include <memory>
#include <string>
#include <vector>

#include <boost/dll.hpp>
#include <spdlog/spdlog.h>

namespace cradle {

class dll_capabilities;
class inner_resources;
class selfreg_seri_catalog;

/*
 * Container of DLLs that are no longer active, but not actually unloaded
 * (see below). Maybe useful for reporting/debugging purposes.
 */
class dll_trash
{
 public:
    void
    add(boost::dll::shared_library* lib)
    {
        libs_.push_back(lib);
    }

    auto
    size() const
    {
        return std::ssize(libs_);
    }

 private:
    std::vector<boost::dll::shared_library*> libs_;
};

/*
 * DLL controller, loading and unloading that DLL.
 *
 * There is a deliberate memory leak here. If we actually unload the DLL,
 * and if any other objects exist referring to code in the DLL, then attempts
 * to execute that code would crash the application; in particular, this holds
 * when calling those other objects' destructors.
 *
 * So we deactivate the DLL rather than unload it, and never call the
 * boost::dll::shared_library destructor. The DLL's resolvers become
 * unavailable for new requests, but the DLL code stays in memory.
 *
 * At least the following references could remain:
 * - The immutable cache contains shared_ptr entries wrapped in an std::any.
 *   When an entry is deleted, the shared_ptr's destructor is called, which
 *   probably is code in the DLL.
 * - A request object's function resides in the DLL.
 * Keeping track of these references is possible but not without cost. In
 * particular, a function_request_erased constructor would need to translate
 * its uuid to some catalog reference, and increase a reference count. This
 * means locking a mutex, whereas creating a request object currently is
 * relatively cheap.
 *
 * A DLL must export (at least) this function:
 * - dll_capabilities const* CRADLE_get_capabilities()
 *   Returns a pointer to a static dll_capabilities object, describing the
 *   DLL's capabilities. Must not return nullptr.
 */
class dll_controller
{
 public:
    // resources and trash must outlive this dll_controller object
    dll_controller(
        inner_resources& resources,
        dll_trash& trash,
        spdlog::logger& logger,
        std::string path,
        std::string name);

    ~dll_controller();

 private:
    inner_resources& resources_;
    dll_trash& trash_;
    spdlog::logger& logger_;
    std::string path_;
    std::string name_;
    // The following is a raw pointer as the destructor must not be called.
    boost::dll::shared_library* lib_{nullptr};
    std::unique_ptr<selfreg_seri_catalog> catalog_;

    void
    load();

    void
    create_seri_catalog(dll_capabilities const& caps);

    void
    unload();
};

} // namespace cradle

#endif
