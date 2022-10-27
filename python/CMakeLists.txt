# Add the integration testing target.
add_custom_target(
    integration_tests
    COMMAND ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${CMAKE_BINARY_DIR}/deploy CRADLE_THINKNODE_API_TOKEN=${API_TOKEN} CRADLE_THINKNODE_DOCKER_AUTH=${DOCKER_AUTH} pytest -s
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS server)
add_test(
    NAME integration_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target integration_tests)

