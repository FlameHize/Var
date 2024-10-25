#include "src/builtin/inside_cmd_service.h"
#include "src/builtin/common.h"
#include "src/server.h"

namespace var {

InsideCmdService::InsideCmdService() {
    ///@todo
}

void InsideCmdService::default_method(net::HttpRequest* request,
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
        server->PrintTabsBody(os, "内部指令");
    }
    response->set_body(os);
}

void InsideCmdService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/inside_cmd";
    info->tab_name = "内部指令";
}

} // end namespace var