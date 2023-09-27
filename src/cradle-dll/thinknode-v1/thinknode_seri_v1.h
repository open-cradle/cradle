#ifndef CRADLE_THINKNODE_SERI_V1_H
#define CRADLE_THINKNODE_SERI_V1_H

#include <atomic>

#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

// The Thinknode seri resolvers are available as long as an instance of this
// object exists.
//
// A DLL will hold a global static instance of this object. The constructor
// runs when the DLL is loaded, the destructor runs on unload. Neither must
// throw an exception.
class thinknode_seri_catalog_v1 : public seri_catalog
{
 public:
    // If auto_register is true, the constructor calls register_all().
    // Thus, auto_register must be false in a DLL, otherwise the exception
    // can/will cause the program to terminate.
    thinknode_seri_catalog_v1(bool auto_register);

    // Registers all Thinknode seri resolvers.
    // Throws on error.
    void
    register_all();

    void
    unregister_all() noexcept override;

 private:
    std::atomic<bool> registered_{false};

    void
    try_register_all();
};

} // namespace cradle

#endif
