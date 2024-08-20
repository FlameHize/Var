enable_testing()

find_package(GTest REQUIRED)
message("GTest include dir: " ${GTEST_INCLUDE_DIRS})
message("GTest libs dir: " ${GTEST_LIBRARIES})

# set(PROJECT_TEST_NAME net_test)
# set(PROJECT_TEST_DIR ${PROJECT_SOURCE_DIR}/net/test)
# file(GLOB TEST_SRC_FILES ${PROJECT_TEST_DIR}/*.cc)

# add_executable(${PROJECT_TEST_NAME} ${TEST_SRC_FILES})
# target_include_directories(${PROJECT_TEST_NAME} PRIVATE 
#                         ${GTEST_INCLUDE_DIRS}
#                         ${PROJECT_SOURCE_DIR}/net)
# target_link_libraries(${PROJECT_TEST_NAME} 
#                     ${GTEST_LIBRARIES} 
#                     var_net
#                     pthread)
# add_test(NAME ${PROJECT_TEST_NAME} COMMAND ${PROJECT_TEST_NAME})

add_executable(buffer_test Buffer_test.cc main.cc)
target_include_directories(buffer_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(buffer_test ${GTEST_LIBRARIES} var_net pthread)

add_executable(channel_test Channel_test.cc main.cc)
target_include_directories(channel_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(channel_test ${GTEST_LIBRARIES} var_net pthread)

add_executable(inetaddress_test InetAddress_test.cc main.cc)
target_include_directories(inetaddress_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(inetaddress_test ${GTEST_LIBRARIES} var_net pthread)

add_executable(timerqueue_test TimerQueue_test.cc main.cc)
target_include_directories(timerqueue_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(timerqueue_test ${GTEST_LIBRARIES} var_net pthread)

add_executable(eventloop_test EventLoop_test.cc main.cc)
target_include_directories(eventloop_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(eventloop_test ${GTEST_LIBRARIES} var_net pthread)

add_executable(eventloopthread_test EventLoopThread_test.cc main.cc)
target_include_directories(eventloopthread_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(eventloopthread_test ${GTEST_LIBRARIES} var_net pthread)

add_executable(eventloopthreadpool_test EventLoopThread_test.cc main.cc)
target_include_directories(eventloopthreadpool_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(eventloopthreadpool_test ${GTEST_LIBRARIES} var_net pthread)

add_executable(tcpclient_test TcpClient_test.cc main.cc)
target_include_directories(tcpclient_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(tcpclient_test ${GTEST_LIBRARIES} var_net pthread)

add_executable(tcpserver_test TcpServer_test.cc main.cc)
target_include_directories(tcpserver_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(tcpserver_test ${GTEST_LIBRARIES} var_net pthread)

add_executable(udpserver_test UdpServer_test.cc main.cc)
target_include_directories(udpserver_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(udpserver_test ${GTEST_LIBRARIES} var_net pthread)

add_executable(httpserver_test HttpServer_test.cc main.cc)
target_include_directories(httpserver_test PRIVATE ${GTEST_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/net)
target_link_libraries(httpserver_test ${GTEST_LIBRARIES} var_net pthread)