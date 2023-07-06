#include <iostream>

#include <cradle/inner/utilities/git.h>

namespace cradle {

// Does the given repository correspond to a tagged version of the code?
static bool
is_tagged_version(repository_info const& info)
{
    return info.commits_since_tag == 0 && !info.dirty;
}

void
show_version_info(repository_info const& info)
{
    if (is_tagged_version(info))
    {
        std::cout << "CRADLE " << info.tag << "\n";
    }
    else
    {
        std::cout << "CRADLE (unreleased version - " << info.commit_object_name
                  << ", " << info.commits_since_tag << " commits ahead of "
                  << info.tag;
        if (info.dirty)
        {
            std::cout << ", with local modifications";
        }
        std::cout << ")\n";
    }
}

} // namespace cradle
