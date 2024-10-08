add_library(var SHARED
    service.cc
    server.cc
    variable.cc
    latency_recorder.cc
    default_variables.cc
    detail/sampler.cc
    detail/percentile.cc
    util/fast_rand.cc
    builtin/jquery_min_js.cc
    builtin/flot_min_js.cc
    builtin/vars_service.cc
    builtin/index_service.cc
    builtin/get_js_service.cc)

target_include_directories(var PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(var var_net)