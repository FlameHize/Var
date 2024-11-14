#include "metric/builtin/heap_profiler_service.h"
#include "metric/builtin/tcmalloc_extension.h"
#include "metric/builtin/common.h"
#include "metric/server.h"

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
    const char* extra_desc = "";
    bool enabled = IsHeapProfilerEnabled();
    if(enabled && !has_TCMALLOC_SAMPLE_PARAMETER()) {
        enabled = false;
        extra_desc = " (no TCMALLOC_SAMPLE_PARAMETER in env)";
    }
    if(!enabled) {
        os << "<pre>\n";
        os << "Error: heap profiler is not enabled." << extra_desc << "\n";
        os << "</pre>\n";
    }
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