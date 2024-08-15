enable_testing()

find_package(GTest REQUIRED)

set(PROJECT_TEST_NAME var_test)
set(TEST_SRC_FILES 
    main.cc
    http_parser_test.cc
    http_method_test.cc
    http_header_test.cc
    http_url_test.cc
    http_status_code_test.cc
    http_message_test.cc)

add_executable(${PROJECT_TEST_NAME} ${TEST_SRC_FILES})
target_include_directories(${PROJECT_TEST_NAME} PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(${PROJECT_TEST_NAME} 
                    ${GTEST_LIBRARIES} 
                    var_net
                    pthread)
