#include <catch2/catch.hpp>
#include <cppcoro/sync_wait.hpp>

#include "../../support/thinknode.h"
#include <cradle/inner/io/mock_http.h>
#include <cradle/thinknode/iam.h>

using namespace cradle;

TEST_CASE("context contents retrieval", "[thinknode][iam]")
{
    thinknode_test_scope scope;

    auto& mock_http = scope.enable_http_mocking();
    mock_http.set_script(
        {{make_get_request(
              "https://mgh.thinknode.io/api/v1.0/iam/contexts/123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"}}),
          make_http_200_response(
              R"(
                        {
                            "bucket": "hacks",
                            "contents": [
                                {
                                    "account": "outatime",
                                    "app": "grays",
                                    "source": {
                                        "version": "1.0.0"
                                    }
                                },
                                {
                                    "account": "chaom",
                                    "app": "landsraad",
                                    "source": {
                                        "branch": "main"
                                }
                                },
                                {
                                    "account": "wayne_enterprises",
                                    "app": "cellsonar",
                                    "source": {
                                        "commit": "a7e1d608d6ce0c25dc6aa597492a6f09"
                                    }
                                }
                            ]
                        }
                    )")}});

    auto ctx{scope.make_context()};
    auto contents = cppcoro::sync_wait(get_context_contents(ctx, "123"));
    REQUIRE(
        contents
        == make_thinknode_context_contents(
            "hacks",
            {make_thinknode_context_app_info(
                 "outatime",
                 "grays",
                 make_thinknode_app_source_info_with_version("1.0.0")),
             make_thinknode_context_app_info(
                 "chaom",
                 "landsraad",
                 make_thinknode_app_source_info_with_branch("main")),
             make_thinknode_context_app_info(
                 "wayne_enterprises",
                 "cellsonar",
                 make_thinknode_app_source_info_with_commit(
                     "a7e1d608d6ce0c25dc6aa597492a6f09"))}));

    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());
}
