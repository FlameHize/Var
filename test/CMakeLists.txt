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
    http_message_test.cc
    string_splitter_test.cc
    linked_list_test.cc
    variable_test.cc
    agent_group_test.cc
    agent_combiner_test.cc
    reducer_test.cc
    sampler_test.cc
    series_test.cc
    window_test.cc
    status_test.cc
    passive_status_test.cc
    average_recorder_test.cc
    percentile_test.cc
    latency_recorder_test.cc
)

add_executable(${PROJECT_TEST_NAME} ${TEST_SRC_FILES})
# 注意这里的头文件问题 http_message_test直接包含http_message.h(间接包含Buffer.h)
# 如果这里不加${PROJECT_SOURCE_DIR}/net 那么CMake无法识别“Buffer.h"路径 只能识别"net/Buffer.h"路径
# 这是因为生成的var_net库的target_include_directories路径级别是PRIVATE的 不对库外文件开放包含的${PROJECT_SOURCE_DIR}/net包含路径
# 这里也可能把var_net的头文件路径改为PUBLIC的 这样下面就不用加${PROJECT_SOURCE_DIR}/net这一行了
target_include_directories(${PROJECT_TEST_NAME} PRIVATE 
                    ${GTEST_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/net)

target_link_libraries(${PROJECT_TEST_NAME} 
                    ${GTEST_LIBRARIES} 
                    var
                    pthread)

# for dummy server running example.
add_executable(dummy_server_case dummy_server_test.cc main.cc)
target_include_directories(dummy_server_case PRIVATE 
                    ${GTEST_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(dummy_server_case
                    ${GTEST_LIBRARIES} 
                    var
                    pthread)
