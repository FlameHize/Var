#include "metric/builtin/profiler_service.h"
#include "metric/builtin/tcmalloc_extension.h"
#include "metric/builtin/common.h"
#include "metric/server.h"
#include <stdlib.h>
#include <unordered_map>
#include <mutex>


namespace var {

enum DisplayType {
    kUnknown,
    kDot,
    kFlameGraph,
    kText
};

static const char* DisplayTypeToString(DisplayType type) {
    switch (type) {
        case DisplayType::kDot: return "dot";
        case DisplayType::kFlameGraph: return "flame";
        case DisplayType::kText: return "text";
        default: return "unknown";
    }
}

static DisplayType StringToDisplayType(const std::string& str) {
    static std::unordered_map<std::string, DisplayType>* display_type_map;
    static std::once_flag flag;
    std::call_once(flag, []() {
        display_type_map = new std::unordered_map<std::string, DisplayType>();
        (*display_type_map)["dot"] = DisplayType::kDot;
        (*display_type_map)["flame"] = DisplayType::kFlameGraph;
        (*display_type_map)["text"] = DisplayType::kText;
    });
    auto type = display_type_map->find(str);
    if(type == display_type_map->end()) {
        return DisplayType::kUnknown;
    }
    return type->second;
}

static const char* ProfilingTypeNameToString(ProfilingType type) {
    switch (type) {
        case PROFILING_CPU: return "CPU分析";
        case PROFILING_HEAP: return "堆内存分析";
    }
    return "unknown";
}

static const char* ProfilingTypePathToString(ProfilingType type) {
    switch (type) {
        case PROFILING_CPU: return "cpu";
        case PROFILING_HEAP: return "heap";
    }
    return "unknown";
}

ProfilerService::ProfilerService() {
    AddMethod("heap", std::bind(&ProfilerService::heap,
        this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("heap_internal", std::bind(&ProfilerService::heap_internal,
        this, std::placeholders::_1, std::placeholders::_2));    
    AddMethod("cpu", std::bind(&ProfilerService::cpu,
        this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("cpu_internal", std::bind(&ProfilerService::cpu_internal,
        this, std::placeholders::_1, std::placeholders::_2));
}

ProfilerService::~ProfilerService() {
}

void ProfilerService::StartProfiling(net::HttpRequest* request,
                                     net::HttpResponse* response,
                                     ProfilingType type) {
    const Server* server = static_cast<Server*>(_owner);
    const bool use_html = UseHTML(request->header());
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
    net::BufferStream os;
    if(use_html) {
        os << "<!DOCTYPE html><html><head>\n"
           << "<meta charset=\"UTF-8\">\n"
           << "<script language=\"javascript\" type=\"text/javascript\" src=\"/js/jquery_min\"></script>\n"
           << TabsHead()
           << "</head><body>";
        server->PrintTabsBody(os, ProfilingTypeNameToString(type));
    }
    
    bool enabled = false;
    const char* error_desc = "";
    if(type == PROFILING_HEAP) {
        enabled = IsHeapProfilerEnabled();
        error_desc = "未找到或加载tcmalloc_and_profiler动态库, 请检查是否安装gperftools，"
        "并确保CMake编译选项中设置-DLINK_TCMALLOC_AND_PROFILER=ON.";
    }
    else {
        ///@todo other profiling type.
    }

    if(!enabled) {
        os << "<pre>Error: " << error_desc << "</pre>\n";
        os << "</body></html>";
        response->set_body(os);
        return;
    }

    // Env parameter setting.
    if(type == PROFILING_HEAP) {
        // Default setting value(512KB) according to gperftools doc.
        setenv("TCMALLOC_SAMPLE_PARAMETER", "524288", 0);
        if(!has_TCMALLOC_SAMPLE_PARAMETER()) {
            os << "<pre>\n";
            os << "错误: 设置内存采样参数TCMALLOC_SAMPLE_PARAMETER失败\n";
            os << "</pre>\n";
            os << "</pre></body></html>";
            response->set_body(os);
            return;
        }
        char* value = getenv("TCMALLOC_SAMPLE_PARAMETER");
        os << "<pre>设置内存采样参数TCMALLOC_SAMPLE_PARAMETER = " << value << "</pre>\n";
    }
    else {
        ///@todo other profiling type.
    }
    
    const char* type_path = ProfilingTypePathToString(type);
    const int seconds = 10;
    const bool show_ccount = request->header().url().GetQuery("ccount");
    const std::string* view = request->header().url().GetQuery("view");
    const std::string* base = request->header().url().GetQuery("base");
    const std::string* display_type_query = request->header().url().GetQuery("display_type");
    DisplayType display_type = DisplayType::kDot;
    if(display_type_query) {
        display_type = StringToDisplayType(*display_type_query);
        if(display_type == DisplayType::kUnknown) {
            os << "<pre>显示类型无效(" << *display_type_query << ")</pre>\n";
            response->set_body(os);
            return; 
        }
        if(display_type == DisplayType::kFlameGraph) {
            setenv("FLAMEGRAPH_PL_PATH", "/usr/local/bin/flamegraph.pl", 0);
        }
    }
    
    if(use_html) {
       os << "<style type=\"text/css\">\n"
            ".logo {position: fixed; bottom: 0px; right: 0px; }\n"
            ".logo_text {color: #B0B0B0; }\n"
            " </style>\n"
            "<script type=\"text/javascript\">\n"
            "function generateURL() {\n"
            "  var past_prof = document.getElementById('view_prof').value;\n"
            "  var base_prof_el = document.getElementById('base_prof');\n"
            "  var base_prof = base_prof_el != null ? base_prof_el.value : '';\n"
            "  var display_type = document.getElementById('display_type').value;\n";
        if(type == PROFILING_CONTENTION) {
            os << "  var show_ccount = document.getElementById('ccount_cb').checked;\n";
        }
        os << "  var targetURL = '/profiler/" << type_path << "';\n"
            "  targetURL += '?display_type=' + display_type;\n"
            "  if (past_prof != '') {\n"
            "    targetURL += '&view=' + past_prof;\n"
            "  }\n"
            "  if (base_prof != '') {\n"
            "    targetURL += '&base=' + base_prof;\n"
            "  }\n";
        if(type == PROFILING_CONTENTION) {
            os <<
            "  if (show_ccount) {\n"
            "    targetURL += '&ccount';\n"
            "  }\n";
        }
        os << "  return targetURL;\n"
            "}\n"
            "$(function() {\n"
            "  function onDataReceived(data) {\n";
        if(view == NULL) {
            os <<
            "    var selEnd = data.indexOf('[addToProfEnd]');\n"
            "    if (selEnd != -1) {\n"
            "      var sel = document.getElementById('view_prof');\n"
            "      var option = document.createElement('option');\n"
            "      option.value = data.substring(0, selEnd);\n"
            "      option.text = option.value;\n"
            "      var slash_index = option.value.lastIndexOf('/');\n"
            "      if (slash_index != -1) {\n"
            "        option.text = option.value.substring(slash_index + 1);\n"
            "      }\n"
            "      var option1 = sel.options[1];\n"
            "      if (option1 == null || option1.text != option.text) {\n"
            "        sel.add(option, 1);\n"
            "      } else if (option1 != null) {\n"
            "        console.log('merged ' + option.text);\n"
            "      }\n"
            "      sel.selectedIndex = 1;\n"
            "      window.history.pushState('', '', generateURL());\n"
            "      data = data.substring(selEnd + '[addToProfEnd]'.length);\n"
            "   }\n";
        }
        os <<
            "    var index = data.indexOf('digraph ');\n"
            "    if (index == -1) {\n"
            "      var selEnd = data.indexOf('[addToProfEnd]');\n"
            "      if (selEnd != -1) {\n"
            "        data = data.substring(selEnd + '[addToProfEnd]'.length);\n"
            "      }\n"
            "      $(\"#profiling-result\").html('<pre>' + data + '</pre>');\n"
            "      if (data.indexOf('FlameGraph') != -1) { init(); }"
            "    } else {\n"
            "      $(\"#profiling-result\").html('Plotting ...');\n"
            "      var svg = Viz(data.substring(index), \"svg\");\n"
            "      $(\"#profiling-result\").html(svg);\n"
            "    }\n"
            "  }\n"
            "  function onErrorReceived(xhr, ajaxOptions, thrownError) {\n"
            "    $(\"#profiling-result\").html(xhr.responseText);\n"
            "  }\n"
            "  $.ajax({\n"
            "    url: \"/profiler/" << type_path << "_internal?console=1";
        if(type == PROFILING_CPU || type == PROFILING_CONTENTION) {
            os << "&seconds=" << seconds;
        }
        // if(profiling_client.id != 0) {
        //     os << "&profiling_id=" << profiling_client.id;
        // }
        os << "&display_type=" << DisplayTypeToString(display_type);
        if(show_ccount) {
            os << "&ccount";
        }
        if(view) {
            os << "&view=" << *view;
        }
        if(base) {
            os << "&base=" << *base;
        }
        os << "\",\n"
            "    type: \"GET\",\n"
            "    dataType: \"html\",\n"
            "    success: onDataReceived,\n"
            "    error: onErrorReceived\n"
            "  });\n"
            "});\n"
            "function onSelectProf() {\n"
            "  window.location.href = generateURL();\n"
            "}\n"
            "function onChangedCB(cb) {\n"
            "  onSelectProf();\n"
            "}\n"
            "</script>\n"
            "</head>\n"
            "<body>\n";
    }

    if(use_html) {
        if(display_type == DisplayType::kDot) {
        // don't need viz.js in text mode.
        os << "<script language=\"javascript\" type=\"text/javascript\""
            " src=\"/js/viz_min\"></script>\n";
        }
    }

    if(use_html) {
        os << "</body></html>";
    }
    response->set_body(os);
}

void ProfilerService::heap(net::HttpRequest* request,
                           net::HttpResponse* response) {
    return StartProfiling(request, response, PROFILING_HEAP);
}

void ProfilerService::cpu(net::HttpRequest* request,
                          net::HttpResponse* response) {
    return StartProfiling(request, response, PROFILING_CPU);
}

void ProfilerService::heap_internal(net::HttpRequest* request,
                                    net::HttpResponse* response) {
    LOG_INFO << request->header().url().path();
}

void ProfilerService::cpu_internal(net::HttpRequest* request,
                                   net::HttpResponse* response) {
    LOG_INFO << request->header().url().path();
}

void ProfilerService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/profiler/heap";
    info->tab_name = "堆内存分析";
    
    info = info_list->add();
    info->path = "/profiler/cpu";
    info->tab_name = "CPU分析";
}

} // end namespace var