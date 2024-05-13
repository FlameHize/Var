enable_testing()

find_package(GTest REQIRED)

set(PROJECT_TEST_NAME var_test)
set(PROJECT_TEST_DIR ${PROJECT_SOURCE_DIR}/test)
file(GLOB TEST_SRC_FILES ${PROJECT_TEST_DIR}/*.cc)