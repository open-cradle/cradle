#ifndef TESTS_EXTERNAL_TEST_SESSION_H
#define TESTS_EXTERNAL_TEST_SESSION_H

#include <cradle/external_api.h>
#include <cradle/inner/io/mock_http.h>
#include <memory>

class external_test_session
{
    cradle::external::api_service service_;
    cradle::external::api_session session_;

 public:
    external_test_session(
        cradle::external::api_service&& service,
        cradle::external::api_session&& session);

    cradle::external::api_session&
    api_session()
    {
        return session_;
    }

    cradle::mock_http_session&
    enable_http_mocking();
};

external_test_session
make_external_test_session();

#endif
