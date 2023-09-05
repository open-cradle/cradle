#ifndef CRADLE_INNER_DLL_DLL_CONTROLLER_H
#define CRADLE_INNER_DLL_DLL_CONTROLLER_H

#include <memory>
#include <string>

#include <boost/dll.hpp>

namespace cradle {

class dll_controller
{
 public:
    dll_controller(std::string path, std::string name);

    void
    load();

    void
    unload();

    std::string const&
    path() const
    {
        return path_;
    }

    std::string const&
    name() const
    {
        return name_;
    }

 private:
    std::string path_;
    std::string name_;
    std::unique_ptr<boost::dll::shared_library> lib_;
};

} // namespace cradle

#endif
