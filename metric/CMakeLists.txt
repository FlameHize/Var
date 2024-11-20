add_library(var SHARED
    server.cc
    variable.cc
    latency_recorder.cc
    default_variables.cc
    detail/sampler.cc
    detail/percentile.cc
    util/fast_rand.cc
    util/tinyxml2.cpp
    util/json.hpp
    builtin/common.cc
    builtin/service.cc
    builtin/jquery_min_js.cc
    builtin/viz_min_js.cc
    builtin/flot_min_js.cc
    builtin/pprof_perl.cc
    builtin/flamegraph_perl.cc
    builtin/vars_service.cc
    builtin/index_service.cc
    builtin/get_js_service.cc
    builtin/log_service.cc
    builtin/inside_cmd_status_user.cc
    builtin/inside_status_service.cc
    builtin/inside_cmd_service.cc
    builtin/file_transfer_service.cc
    builtin/xvc_service.cc
    builtin/remote_sampler_service.cc
    builtin/tcmalloc_extension.cc
    builtin/profiler_service.cc)

target_include_directories(var PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(var 
                      var_net
                      dl)

option(LINK_TCMALLOC_AND_PROFILER "Whether linked tcmalloc_and_profiler lib" ON)
if(LINK_TCMALLOC_AND_PROFILER)
    find_path(GPERFTOOLS_INCLUDE_DIR NAMES gperftools/heap-profiler.h)
    find_library(GPERFTOOLS_LIBRARIES NAMES tcmalloc_and_profiler)
    if((NOT GPERFTOOLS_INCLUDE_DIR) OR (NOT GPERFTOOLS_LIBRARIES))
        message(FATAL_ERROR "Failed to find tcmalloc_and_profiler")
    else()
        message(STATUS "Found tcmalloc_and_profiler:" ${GPERFTOOLS_LIBRARIES})
        target_include_directories(var PRIVATE ${GPERFTOOLS_INCLUDE_DIR})
        target_link_libraries(var ${GPERFTOOLS_LIBRARIES})
    endif()
else()
endif()
# include_directories(${GPERFTOOLS_INCLUDE_DIR})