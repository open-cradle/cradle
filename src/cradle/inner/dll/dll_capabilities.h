#ifndef CRADLE_INNER_DLL_DLL_CAPABILITIES_H
#define CRADLE_INNER_DLL_DLL_CAPABILITIES_H

#include <memory>

namespace cradle {

class selfreg_seri_catalog;
class seri_registry;

// Encodes the capabilities that a DLL offers.
// The DLL exports a static instance of this class.
// The class is open to extension.
class dll_capabilities
{
 public:
    // Optional capability to create a seri_catalog.
    using create_seri_catalog_t
        = std::unique_ptr<selfreg_seri_catalog>(seri_registry& registry);
    create_seri_catalog_t* create_seri_catalog{nullptr};
};

} // namespace cradle

#endif
