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

extern "C" {
    int __attribute__((weak)) ProfilerStart(const char* fname);
    void __attribute__((weak)) ProfilerStop();
}

namespace var {

const std::string ProfilerFileSaveDir = program_work_dir("bin") + "data/profiler/";
static const char* const PPROF_FILENAME = "pprof.pl";
static const char* const FLAMEGRAPH_FILENAME = "flamegraph.pl";
static std::once_flag g_written_pprof_perl;
static std::once_flag g_written_flamegraph_perl;
static int FLAMEGRAPH_DISPLAY_WIDTH = 1600;
static int MAX_PROFILES_KEPT = 8;
static int DEFAULT_PROFILING_SECONDS = 10;

// Set in ProfilerLinker.
bool cpu_profiler_enabled = false;

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
        case PROFILING_CPU: return "CPU性能分析";
        case PROFILING_HEAP: return "CPU内存分析";
        case PROFILING_GROWTH: return "CPU内存优化";
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

// NOTE: This function MUST be applied to all parameters finally passed to
// system related functions (popen/system/exec ...) to avoid potential
// injections from URL and other user inputs
static bool VaildProfilePath(const std::string& path) {
    auto starts_with = [](const std::string& lhs, const std::string& rhs) -> bool {
        return ( lhs.length() >= rhs.length() && 
        std::string::traits_type::compare(lhs.data(), rhs.data(), rhs.length()) == 0 );
    };
    if(!starts_with(path, ProfilerFileSaveDir)) {
        // Must be under the directory.
        return false;
    }
    int consecutive_dot_count = 0;
    for(size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];
        if(c == '.') {
            ++consecutive_dot_count;
            if(consecutive_dot_count >= 2) {
                // Disallow consective dots to go to upper level directories.
                return false;
            }
            else {
                continue;
            }
        }
        else {
            consecutive_dot_count = 0;
        }
        if(!isalpha(c) && !isdigit(c) &&
            c != '_' && c != '-' && c != '/') {
            return false;
        }
    }
    return true;
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
    AddMethod("growth", std::bind(&ProfilerService::growth,
        this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("growth_internal", std::bind(&ProfilerService::growth_internal,
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
    const std::string* base_name = request->header().url().GetQuery("base");
    const std::string* display_type_query = request->header().url().GetQuery("display_type");
    DisplayType display_type = DisplayType::kDot;
    if(display_type_query) {
        display_type = StringToDisplayType(*display_type_query);
        if(display_type == DisplayType::kUnknown) {
            os << "Invalid display type=" << *display_type_query;
            response->header().set_status_code(net::HTTP_STATUS_INTERNAL_SERVER_ERROR);
            response->set_body(os);
            return; 
        }
    }

    if(base_name) {
        if(!VaildProfilePath(*base_name)) {
            os << "Invalid query 'base'";
            response->header().set_status_code(net::HTTP_STATUS_INTERNAL_SERVER_ERROR);
            response->set_body(os);
            return;
        }
        FileReaderLinux base_file(base_name->c_str(), "r");
        if(!base_file.get()) {
            os << "The profile denoted by 'base' does not exist";
            response->header().set_status_code(net::HTTP_STATUS_INTERNAL_SERVER_ERROR);
            response->set_body(os);
            return;
        }
    }

    // Try to read cache first.
    char expected_result_name[256];
    MakeCacheName(expected_result_name, sizeof(expected_result_name),
                  prof_name, GetBaseName(base_name),
                  display_type, show_ccount);
    std::string cache_result;
    if(FileUtil::readFile<std::string>(expected_result_name, 
        64 * 1024 * 1024, &cache_result) == 0) {
        os << "<pre>" << cache_result << "</pre></body></html>";
        response->set_body(os);
        return;
    }

    // Build cmd sub process.
    std::ostringstream cmd_builder;
    std::string pprof_tool_path(GeneratePerlScriptPath(PPROF_FILENAME));
    std::string flamegraph_tool_path(GeneratePerlScriptPath(FLAMEGRAPH_FILENAME));
    cmd_builder << "perl " << pprof_tool_path
                << DisplayTypeToPProfArgument(display_type)
                << ((show_ccount || type == PROFILING_IOBUF) ? " --contention " : "");
    if(base_name) {
        cmd_builder << "--base " << *base_name << ' ';
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
                    GetBaseName(base_name), display_type, show_ccount);

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
    if(base_name) {
        tmp.append(" - ");
        tmp.append(GetBaseName(base_name));
    }
    tmp.append("]\n");
    tmp.append(prof_result);
    tmp.swap(prof_result);

    net::Buffer prof_result_copy = prof_result;
    if(!WriteProfDataToFile(result_name, prof_result_copy.retrieveAllAsString())) {
        LOG_ERROR << "Failed to write " << result_name;
        std::string cache_file_dir;
        DirReaderLinux::GetParentDirectory(result_name, cache_file_dir);
        DirReaderLinux::DeleteDirectory(cache_file_dir.c_str());
    }
    os.moveTo(resp);
    resp.append(prof_result);
    if(use_html) {
        resp.append("</body></html>");
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

    const std::string* view = request->header().url().GetQuery("view");
    if(view) {
        if(!VaildProfilePath(*view)) {
            os << "Invalid query 'view'\n";
            response->header().set_status_code(net::HTTP_STATUS_FORBIDDEN);
            response->set_body(os);
            return;
        }
        FileReaderLinux view_file(view->c_str(), "r");
        if(!view_file.get()) {
            os << "The profile denoted by 'view' does not existed";
            response->header().set_status_code(net::HTTP_STATUS_FORBIDDEN);
            response->set_body(os);
            return;
        }
        DisplayProfiling(request, response, view->c_str(), os.buf(), type);
        return;
    }

    const std::string* seconds_str = request->header().url().GetQuery("seconds");
    int seconds = DEFAULT_PROFILING_SECONDS;
    if(seconds_str) {
        seconds = std::stoi(*seconds_str);
    }

    std::string prof_name = MakeProfName(type);
    if(type == PROFILING_HEAP) {
        MallocExtension* malloc_ext = MallocExtension::instance();
        if(!malloc_ext || !has_TCMALLOC_SAMPLE_PARAMETER()) {
            os << "Heap profiler is not enabled";
            if(malloc_ext) {
                os << " (No TCMALLOC_SAMPLE_PARAMETER in env, please set it)";
            }
            response->header().set_status_code(net::HTTP_STATUS_FORBIDDEN);
            response->set_body(os);
            return;
        }
        std::string obj;
        malloc_ext->GetHeapSample(&obj);
        if(!WriteProfDataToFile(prof_name.c_str(), obj)) {
            os << "Fail to write " << prof_name;
            response->header().set_status_code(net::HTTP_STATUS_FORBIDDEN);
            response->set_body(os);
            return;
        }
    }
    else if(type == PROFILING_GROWTH) {
        MallocExtension* malloc_ext = MallocExtension::instance();
        if(!malloc_ext || !has_TCMALLOC_SAMPLE_PARAMETER()) {
            os << "Growth profiler is not enabled";
            if(malloc_ext) {
                os << " (No TCMALLOC_SAMPLE_PARAMETER in env, please set it)";
            }
            response->header().set_status_code(net::HTTP_STATUS_FORBIDDEN);
            response->set_body(os);
            return;
        }
        std::string obj;
        malloc_ext->GetHeapGrowthStacks(&obj);
        if(!WriteProfDataToFile(prof_name.c_str(), obj)) {
            os << "Fail to write " << prof_name;
            response->header().set_status_code(net::HTTP_STATUS_FORBIDDEN);
            response->set_body(os);
            return;
        }
    }
    else if(type == PROFILING_CPU) {
        if((void*)ProfilerStart == nullptr || (void*)ProfilerStop == nullptr) {
            os << "CPU profiler is not enabled";
            response->header().set_status_code(net::HTTP_STATUS_INTERNAL_SERVER_ERROR);
            response->set_body(os);
            return;
        }
        std::string cpu_path = ProfilerFileSaveDir + "cpu";
        DirReaderLinux::CreateDirectoryIfNotExists(cpu_path.c_str());
        if(!ProfilerStart(prof_name.c_str())) {
            os << "Another profiler (not via /profiler/cpu) is running, try again later";
            response->header().set_status_code(net::HTTP_STATUS_SERVICE_UNAVAILABLE);
            response->set_body(os);
            return;
        }
        usleep(seconds * 1000000L);
        ProfilerStop();
    }
    else {
        ///@todo other profiler type.
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
    const char* error_desc = "未找到或加载tcmalloc_and_profiler动态库, 请检查是否安装gperftools，"
    "并确保CMake编译选项中设置-DLINK_TCMALLOC_AND_PROFILER=ON.";
    if(type == PROFILING_HEAP || type == PROFILING_GROWTH) {
        // Default: export TCMALLOC_SAMPLE_PARAMETER=524288(512KB)
        enabled = IsHeapProfilerEnabled();
    }
    else if(type == PROFILING_CPU) {
        // Default: export CPUPROFILE_FREQUENCY=100
        enabled = cpu_profiler_enabled;
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
    if(type == PROFILING_HEAP || type == PROFILING_CPU || type == PROFILING_GROWTH) {
        // Default setting value(512KB) according to gperftools doc.
        // setenv("TCMALLOC_SAMPLE_PARAMETER", "524288", 0);
        if(!has_TCMALLOC_SAMPLE_PARAMETER()) {
            os << "<pre>\n";
            os << "错误: 未设置内存采样参数TCMALLOC_SAMPLE_PARAMETER，请设置(export TCMALLOC_SAMPLE_PARAMETER = 524288)\n";
            os << "</pre>\n";
            os << "</pre></body></html>";
            response->set_body(os);
            return;
        }
    }
    else if(type == PROFILING_CPU) {
        if(!getenv("CPUPROFILE_FREQUENCY")) {
            os << "<pre>\n";
            os << "错误: 未设置CPU采样频率CPUPROFILE_FREQUENCY，请设置(export CPUPROFILE_FREQUENCY = 100)\n";
            os << "</pre>\n";
            os << "</pre></body></html>";
            response->set_body(os);
            return;
        }
    }
    else {
        ///@todo other profiling type.
    }

    char* TCMALLOC_SAMPLE_PARAMETER = getenv("TCMALLOC_SAMPLE_PARAMETER");
    char* CPUPROFILE_FREQUENCY = getenv("CPUPROFILE_FREQUENCY");
    os << "<style>\n"
    ".right-bottom {\n"
    "   position: fixed;\n"
    "   bottom: 0;\n"
    "   left: 0;\n"
    "   padding: 0px;\n"
    "   z-index: 1000;\n"
    "}\n"
    "</style>\n";
    os << "<div class=\"right-bottom\">\n";
    if(TCMALLOC_SAMPLE_PARAMETER) {
        os << "<p>TCMALLOC_SAMPLE_PARAMETER: " << TCMALLOC_SAMPLE_PARAMETER << "</p>\n";
    }
    if(CPUPROFILE_FREQUENCY) {
        os << "<p>CPUPROFILE_FREQUENCY: " << CPUPROFILE_FREQUENCY << "</p>\n";
    }
    os << "</div>\n";

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
    }

    const std::string* seconds_str = request->header().url().GetQuery("seconds");
    int seconds = DEFAULT_PROFILING_SECONDS;
    if(seconds_str) {
        seconds = std::stoi(*seconds_str);
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
        if(type == PROFILING_CPU) {
            os << "  var seconds = document.getElementById('seconds').value;\n";
        }
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
        if(type == PROFILING_CPU) {
            os <<
            " targetURL += '&seconds=' + seconds;\n";
        }
        if(type == PROFILING_CONTENTION) {
            os <<
            "  if (show_ccount) {\n"
            "    targetURL += '&ccount';\n"
            "  }\n";
        }
        os << "  return targetURL;\n"
            "}\n"
            // "$(function() {\n"
            "function startProfiling() {\n"
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
            "    $(\"#profiling-prompt\").html('Profiling success');\n"
            "  }\n"
            "  function onErrorReceived(xhr, ajaxOptions, thrownError) {\n"
            "    $(\"#profiling-result\").html(xhr.responseText);\n"
            "    $(\"#profiling-prompt\").html('Profiling fail');\n"
            "  }\n"
            "  $(\"#profiling-prompt\").html('Generating profile ";
        if(type == PROFILING_CPU && !view) {
            os << "for " << seconds << " seconds";
        }
        os << " ......');\n";
        os << 
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
            // "});\n"
            "}\n"
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

    std::vector<std::string> past_profs;
    std::string prof_dir(ProfilerFileSaveDir);
    prof_dir.append(type_path);
    if(DirReaderLinux::DirectoryExists(prof_dir.c_str())) {
        DirReaderLinux::ListChildFiles(prof_dir.c_str(), past_profs);
    }
    // Cached files in MAX_PROFILE_KEPT limit size if new profile coming.
    if(!view && !past_profs.empty()) {
        std::sort(past_profs.begin(), past_profs.end(), std::greater<std::string>());
        size_t max_profiles = MAX_PROFILES_KEPT;
        if(past_profs.size() >= max_profiles) {
            LOG_TRACE << "Remove " << past_profs.size() - max_profiles << " profiles file";
            for(size_t i = max_profiles - 1; i < past_profs.size(); ++i) {
                remove(past_profs[i].c_str());
                std::string prof_cache_file = past_profs[i];
                prof_cache_file.append(".cache");
                DirReaderLinux::DeleteDirectory(prof_cache_file.c_str());
            }
            past_profs.resize(max_profiles - 1);
        }
    }

    os << "<pre style='display:inline'>视图: </pre>\n"
    "<select id='view_prof' onchange='onSelectProf()'>\n"
    "<option value=''>&lt;new profile&gt;</option>\n";
    for(size_t i = 0; i < past_profs.size(); ++i) {
        os << "<option value='" << past_profs[i] << "' ";
        if(view && past_profs[i] == *view) {
            os << "selected";
        }
        os << '>' << GetBaseName(past_profs[i].c_str()) << "\n";
    }
    os << "</select>\n";

    os << "<pre style='display:inline'>基准: </pre>"
    "<select id='base_prof' onchange='onSelectProf()'>\n"
    "<option value=''>&lt;non profile&gt;</option>\n";
    for(size_t i = 0; i < past_profs.size(); ++i) {
        os << "<option value='" << past_profs[i] << "' ";
        if(base && past_profs[i] == *base) {
            os << "selected";
        }
        os << '>' << GetBaseName(past_profs[i].c_str()) << "\n";
    }
    os << "</select>\n";

    os << "<pre style='display:inline'>绘制方式: </pre>"
    "<select id='display_type' onchange='onSelectProf()'>\n"
    "<option value=dot" << (display_type == DisplayType::kDot ? " selected" : "") << ">拓扑图</option>\n"
    "<option value=flame" << (display_type == DisplayType::kFlameGraph ? " selected" : "") << ">火焰图</option>\n"
    "<option value=text" << (display_type == DisplayType::kText ? " selected" : "") << ">文本</option>\n"
    "</select>\n";

    if(type == PROFILING_CPU) {
        std::vector<int> seconds_list = {10, 20, 30, 60};
        os << "<pre style='display:inline'>时长: </pre>"
        "<select id='seconds' onchange='onSelectProf()'>\n";
        for(size_t i = 0; i < seconds_list.size(); ++i) {
            os << "<option value='" << seconds_list[i] << "' ";
            if(seconds == seconds_list[i]) {
                os << "selected";
            }
            os << '>' << seconds_list[i] << "s" << "</option>\n";
        }
        os << "</select>\n";
    }

    os << "&nbsp;<button type=\"button\" onclick=\"startProfiling()\">快照</button>\n";

    os << "<div id=\"profiling-prompt\"></div>\n";
    os << "<div id=\"profiling-result\"></div>\n";

    if(use_html) {
        if(display_type == DisplayType::kDot) {
        // Only need viz.js in Dot mode.
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

void ProfilerService::growth(net::HttpRequest* request,
                             net::HttpResponse* response) {
    return StartProfiling(request, response, PROFILING_GROWTH);
}

void ProfilerService::growth_internal(net::HttpRequest* request,
                                      net::HttpResponse* response) {
    return DoProfiling(request, response, PROFILING_GROWTH);
}

void ProfilerService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/profiler/cpu";
    info->tab_name = ProfilingTypeNameToString(PROFILING_CPU);

    info = info_list->add();
    info->path = "/profiler/heap";
    info->tab_name = ProfilingTypeNameToString(PROFILING_HEAP);
    
    info = info_list->add();
    info->path = "/profiler/growth";
    info->tab_name = ProfilingTypeNameToString(PROFILING_GROWTH);
}

} // end namespace var