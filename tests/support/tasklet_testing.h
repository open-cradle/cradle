#ifndef CRADLE_TESTS_SUPPORT_TASKLET_TESTING_H
#define CRADLE_TESTS_SUPPORT_TASKLET_TESTING_H

#include <cradle/inner/introspection/tasklet_impl.h>

namespace cradle {

/**
 * Fixture providing a clean tasklet_admin state
 *
 * Should be added at the start of TEST_CASEs that access the tasklet_admin
 * instance; in particular, the ones that call create_tasklet_tracker() or
 * get_tasklet_infos().
 *
 * The constructor provides a clean, well-defined instance to work on. The
 * destructor cleans up the instance to have fewer reported memory leaks.
 */
struct clean_tasklet_admin_fixture
{
    clean_tasklet_admin_fixture(bool initially_enabled = true)
    {
        tasklet_admin::instance().hard_reset_testing_only(initially_enabled);
    }

    ~clean_tasklet_admin_fixture()
    {
        tasklet_admin::instance().hard_reset_testing_only();
    }
};

} // namespace cradle

#endif
