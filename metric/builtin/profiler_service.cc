#include "metric/builtin/profiler_service.h"
#include "metric/builtin/tcmalloc_extension.h"
#include "metric/builtin/pprof_perl.h"
#include "metric/builtin/flamegraph_perl.h"
#include "metric/builtin/common.h"
#include "metric/util/dir_reader_linux.h"
#include "metric/util/file_reader_linux.h"
#include "metric/util/cmd_reader_linux.h"
#include "metric/server.h"
#include "net/base/FileUtil.h"
#include <stdlib.h>
#include <unordered_map>
#include <mutex>


namespace var {

const std::string ProfilerFileSaveDir = program_work_dir("bin") + "data/profiler/";
static const char* const PPROF_FILENAME = "pprof.pl";
static const char* const FLAMEGRAPH_FILENAME = "flamegraph.pl";
static std::once_flag g_written_pprof_perl;
static std::once_flag g_written_flamegraph_perl;
static int FLAMEGRAPH_DISPLAY_WIDTH = 1600;

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
        case PROFILING_GROWTH: return "growth";
        case PROFILING_CONTENTION: return "contention";
        case PROFILING_IOBUF: return "iobuf";
    }
    return "unknown";
}

static const char* ProfilingTypePathToString(ProfilingType type) {
    switch (type) {
        case PROFILING_CPU: return "cpu";
        case PROFILING_HEAP: return "heap";
        case PROFILING_GROWTH: return "growth";
        case PROFILING_CONTENTION: return "contention";
        case PROFILING_IOBUF: return "iobuf";
    }
    return "unknown";
}

static int MakeCacheName(char* cache_name, size_t len,
                         const char* prof_name,
                         const char* base_name,
                         DisplayType display_type,
                         bool show_ccount) {
    if(base_name) {
        return snprintf(cache_name, len, "%s.cache/base_%s.%s%s", prof_name,
                        base_name,
                        DisplayTypeToString(display_type),
                        (show_ccount ? ".ccount" : ""));
    } 
    else {
        return snprintf(cache_name, len, "%s.cache/%s%s", prof_name,
                        DisplayTypeToString(display_type),
                        (show_ccount ? ".ccount" : ""));

    }
}

static std::string MakeProfName(ProfilingType type) {
    std::string prof_name = ProfilerFileSaveDir;
    prof_name += ProfilingTypePathToString(type);
    prof_name += '/';

    char time_buf[128];
    time_t rawtime;
    time(&rawtime);
    struct tm* timeinfo = localtime(&rawtime);
    strftime(time_buf, 128, "%Y%m%d.%H%M%S", timeinfo);
    std::string timestamp(time_buf);
    
    prof_name += timestamp;
    return prof_name;
}

static bool WriteProfDataToFile(const char* file_path, const std::string& content) {
    std::string file_dir;
    DirReaderLinux::GetParentDirectory(file_path, file_dir);
    if(!DirReaderLinux::CreateDirectoryIfNotExists(file_dir.c_str())) {
        LOG_ERROR << "Failed to create directory: " << file_dir;
        return false;
    }
    FileUtil::AppendFile file(file_path);
    file.append(content.data(), content.length());
    file.flush();
    return true;
}

static std::string DisplayTypeToPProfArgument(DisplayType type) {
    switch (type) {
        case DisplayType::kDot: return " --dot ";
        case DisplayType::kFlameGraph: return " --collapsed ";
        case DisplayType::kText: return " --text ";
        default: return " unknown type ";
    }
}

static std::string GeneratePerlScriptPath(const std::string& filename) {
    std::string path;
    path.reserve(ProfilerFileSaveDir.size() + 1 + filename.size());
    path += ProfilerFileSaveDir;
    path += filename;
    return path;
}

static const char* GetBaseName(const std::string* full_base_name) {
    if(!full_base_name) {
        return nullptr;
    }
    size_t offset = full_base_name->find_last_of('/');
    if(offset == std::string::npos) {
        offset = 0;
    }
    else {
        ++offset;
    }
    return full_base_name->c_str() + offset;
}

static const char* GetBaseName(const char* full_base_name) {
    std::string s(full_base_name);
    size_t offset = s.find_last_of('/');
    if (offset == std::string::npos) {
        offset = 0;
    } else {
        ++offset;
    }
    return s.data() + offset;
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

void ProfilerService::DisplayProfiling(net::HttpRequest* request,
                                       net::HttpResponse* response,
                                       const char* prof_name,
                                       net::Buffer& result_prefix,
                                       ProfilingType type) {
    const bool use_html = UseHTML(request->header());
    net::Buffer prof_result;
    net::Buffer& resp = response->body();
    net::BufferStream os;
    
    const bool show_ccount = request->header().url().GetQuery("ccount");
    const std::string* view = request->header().url().GetQuery("view");
    const std::string* base = request->header().url().GetQuery("base");
    const std::string* display_type_query = request->header().url().GetQuery("display_type");
    DisplayType display_type = DisplayType::kText;
    if(display_type_query) {
        display_type = StringToDisplayType(*display_type_query);
        if(display_type == DisplayType::kUnknown) {
            os << "<pre>显示类型无效(" << *display_type_query << ")</pre>\n";
            response->set_body(os);
            return; 
        }
    }

    if(base) {
        ///@todo 
    }

    ///@todo cache.


    // Build cmd sub process.
    std::ostringstream cmd_builder;
    std::string pprof_tool_path(GeneratePerlScriptPath(PPROF_FILENAME));
    std::string flamegraph_tool_path(GeneratePerlScriptPath(FLAMEGRAPH_FILENAME));
    cmd_builder << "perl " << pprof_tool_path
                << DisplayTypeToPProfArgument(display_type)
                << ((show_ccount || type == PROFILING_IOBUF) ? " --contention " : "");
    if(base) {
        cmd_builder << "--base " << *base << ' ';
    }

    cmd_builder << program_name() << " " << prof_name; 
    if(display_type == DisplayType::kFlameGraph) {
        // For flamegraph, we don't care about pprof error msg,
        // which will cause confusing messages in the final result.
        cmd_builder << " 2>/dev/null  | perl " <<  flamegraph_tool_path  << " --width " 
                    << (FLAMEGRAPH_DISPLAY_WIDTH < 1200 ? 1200 : FLAMEGRAPH_DISPLAY_WIDTH);
    }
    cmd_builder << " 2>&1 ";
    const std::string cmd = cmd_builder.str();

    net::BufferStream pprof_output;
    LOG_INFO << "Running cmd = " << cmd;
    const int rc = ReadCommandOutput(pprof_output, cmd.c_str());
    if(rc != 0) {
        if(rc < 0) {
            os << "Failed to execute '" << cmd << "',"
                << (use_html ? "</body></html>" : "\n");
            response->set_body(os);
            response->header().set_status_code(net::HTTP_STATUS_INTERNAL_SERVER_ERROR);
            return;
        }
    }
    pprof_output.moveTo(prof_result);
    
    // Cache result in file.
    char result_name[256];
    MakeCacheName(result_name, sizeof(result_name), prof_name,
                    GetBaseName(base), display_type, show_ccount);

    // Append the profile name as the visual reminder for what current profile is.
    net::Buffer before_label;
    net::Buffer tmp;
    if(!view) {
        tmp.append(prof_name);
        tmp.append("[addToProfEnd]");
    }
    // Assume it's text, append before result directly.
    tmp.append("[");
    tmp.append(GetBaseName(prof_name));
    if(base) {
        tmp.append(" - ");
        tmp.append(GetBaseName(base));
    }
    tmp.append("]\n");
    tmp.append(prof_result);
    tmp.swap(prof_result);

    net::Buffer prof_result_copy = prof_result;
    if(!WriteProfDataToFile(result_name, prof_result_copy.retrieveAllAsString())) {
        LOG_ERROR << "Failed to write " << result_name;
        ///@todo delete file.
    }

    os.moveTo(resp);
    if(use_html) {
        resp.append("<pre>");
    }
    resp.append(prof_result);
    if(use_html) {
        resp.append("</pre></body></html>");
    }
    return;
}

void ProfilerService::DoProfiling(net::HttpRequest* request,
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

    std::string prof_name = MakeProfName(type);
    LOG_INFO << prof_name;
    if(type == PROFILING_HEAP) {
        MallocExtension* malloc_ext = MallocExtension::instance();
        if(!malloc_ext || !has_TCMALLOC_SAMPLE_PARAMETER()) {
            os << "Heap profiler is not enabled";
            if(malloc_ext) {
                os << " (No TCMALLOC_SAMPLE_PARAMETER in env, please set it)";
            }
            os << '.' << (use_html ? "</body></html>" : "\n");
            response->header().set_status_code(net::HTTP_STATUS_FORBIDDEN);
            response->set_body(os);
            return;
        }
        std::string obj;
        malloc_ext->GetHeapSample(&obj);
        sleep(10);
        if(!WriteProfDataToFile(prof_name.c_str(), obj)) {
            os << "Fail to write " << prof_name
               << (use_html ? "</body></html>" : "\n");
            response->header().set_status_code(net::HTTP_STATUS_FORBIDDEN);
            response->set_body(os);
            return;
        }
    }
    else if(type == PROFILING_CPU) {

    }
    else {

    }
    DisplayProfiling(request, response, prof_name.c_str(), os.buf(), type);
    return;
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
        // setenv("TCMALLOC_SAMPLE_PARAMETER", "524288", 0);
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

    // Written pprof.pl and flamegraph.pl file into profiler.
    bool pprof_tool_flag = true;
    std::call_once(g_written_pprof_perl, [&pprof_tool_flag]() {
        std::string pprof_tool_path = GeneratePerlScriptPath(PPROF_FILENAME);
        FileReaderLinux pprof_file(pprof_tool_path.c_str(), "r");
        if(pprof_file.get()) {
            return;
        }
        if(!WriteProfDataToFile(pprof_tool_path.c_str(), pprof_perl())) {
            pprof_tool_flag = false;
        }
    });
    if(!pprof_tool_flag) {
        os << "<pre>写入pprof.pl脚本文件失败</pre>\n";
        response->set_body(os);
        return;
    }

    bool flamegraph_tool_flag = true;
    std::call_once(g_written_flamegraph_perl, [&flamegraph_tool_flag] () {
        std::string flamegraph_tool_path = GeneratePerlScriptPath(FLAMEGRAPH_FILENAME);
        FileReaderLinux flamegraph_file(flamegraph_tool_path.c_str(), "r");
        if(flamegraph_file.get()) {
            return;
        }
        if(!WriteProfDataToFile(flamegraph_tool_path.c_str(), flamegraph_perl())) {
            flamegraph_tool_flag = false;
        }
    });
    if(!flamegraph_tool_flag) {
        os << "<pre>写入flamegraph.pl脚本文件失败</pre>\n";
        response->set_body(os);
        return;
    }

    const char* type_path = ProfilingTypePathToString(type);
    const int seconds = 10;
    const bool show_ccount = request->header().url().GetQuery("ccount");
    const std::string* view = request->header().url().GetQuery("view");
    const std::string* base = request->header().url().GetQuery("base");
    const std::string* display_type_query = request->header().url().GetQuery("display_type");
    DisplayType display_type = DisplayType::kFlameGraph;
    if(display_type_query) {
        display_type = StringToDisplayType(*display_type_query);
        if(display_type == DisplayType::kUnknown) {
            os << "<pre>显示类型无效(" << *display_type_query << ")</pre>\n";
            response->set_body(os);
            return; 
        }
    }

    ///@todo ProfilingClient used to cache ProfilingResult in multi-thread env.
    
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
            "console.log(data.length);\n"
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
            "    url: \"/profiler/" << type_path << "_internal?console=0";
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

    ///@todo Already existed file list.
    std::vector<std::string> past_profs;

    os << "<pre style='display:inline'>View: </pre>"
    "<select id='view_prof' onchange='onSelectProf()'>\n";
    for(size_t i = 0; i < past_profs.size(); ++i) {
        os << "<option value='" << past_profs[i] << "' ";
        if(view && past_profs[i] == *view) {
            os << "selected";
        }
        os << '>' << past_profs[i] << "\n";
    }
    os << "</select>\n";

    os << "<div><pre style='display:inline'>Display: </pre>"
    "<select id='display_type' onchange='onSelectProf()'>\n"
    "<option value=dot" << (display_type == DisplayType::kDot ? "selected" : "") << ">dot</option>\n"
    "option value=flame" << (display_type == DisplayType::kFlameGraph ? "selected" : "") << ">flame</option>\n"\
    "option value=text" << (display_type == DisplayType::kText ? "selected" : "") << "text</option>\n";
    os << "</select></div>\n";

    os << "<div><pre style='display:inline'>Diff: </pre>"
    "<select id='base_prof' onchange='onSelectProf()'>\n"
    "<option value=''>&lt;none&gt;</option>\n";
    for(size_t i = 0; i < past_profs.size(); ++i) {
        os << "<option value='>" << past_profs[i] << "' ";
        if(base && past_profs[i] == *base) {
            os << "selected";
        }
        os << '>' << past_profs[i] << "\n";
    }
    os << "</select></div>\n";

    os << "<div id=\"profiling-result\"></div>\n";
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
    return DoProfiling(request, response, PROFILING_HEAP);
}

void ProfilerService::cpu_internal(net::HttpRequest* request,
                                   net::HttpResponse* response) {
    return DoProfiling(request, response, PROFILING_CPU);
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