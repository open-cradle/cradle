#ifndef CRADLE_TYPING_UTILITIES_GIT_H
#define CRADLE_TYPING_UTILITIES_GIT_H

#include <string>

namespace cradle {

// This captures information about the state of the Git repository for the
// source code itself.
struct repository_info
{
    // the abbreviated object name of the current commit
    std::string commit_object_name;

    // Does the repository have local modifications?
    bool dirty;

    // the closest tag in the history of the repository
    std::string tag;

    // how many commits there have been since the tag
    unsigned commits_since_tag;
};

void
show_version_info(repository_info const& info);

} // namespace cradle

#endif
