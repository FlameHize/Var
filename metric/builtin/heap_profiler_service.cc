#include "metric/builtin/heap_profiler_service.h"
#include "metric/builtin/tcmalloc_extension.h"
#include "metric/builtin/common.h"
#include "metric/server.h"
#include <stdlib.h>

namespace var {

HeapProfilerService::HeapProfilerService() {
}

HeapProfilerService::~HeapProfilerService() {
}

void HeapProfilerService::default_method(net::HttpRequest* request,
                                         net::HttpResponse* response) {
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
        server->PrintTabsBody(os, "堆内存分析");
    }
    bool enabled = IsHeapProfilerEnabled();
    if(!enabled) {
        os << "<pre>\n";
        os << "错误: 未找到或加载tcmalloc_and_profiler动态库, 请检查是否安装gperftools，"
              "并确保CMake编译选项中设置-DLINK_TCMALLOC_AND_PROFILER=ON.\n";
        os << "</pre>\n";
        os << "</pre></body></html>";
        response->set_body(os);
        return;
    }
    // Default setting according to gperftools doc.
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
    os << "<pre>启动成功，内存采样参数TCMALLOC_SAMPLE_PARAMETER=" << value << "</pre>\n";
    if(use_html) {
        os << "</pre></body></html>";
    }
    response->set_body(os);
}

void HeapProfilerService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/heap_profiler";
    info->tab_name = "堆内存分析";
}

} // end namespace var