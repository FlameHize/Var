add_executable(tcp_throughput_measure_server server.cc)
target_include_directories(tcp_throughput_measure_server PRIVATE 
                    ${GTEST_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(tcp_throughput_measure_server
                    ${GTEST_LIBRARIES} 
                    var
                    pthread)

add_executable(tcp_throughput_measure_client client.cc)
target_include_directories(tcp_throughput_measure_client PRIVATE 
                    ${GTEST_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(tcp_throughput_measure_client
                    ${GTEST_LIBRARIES} 
                    var
                    pthread)