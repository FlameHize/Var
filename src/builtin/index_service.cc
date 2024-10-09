#include "src/builtin/index_service.h"
#include "src/builtin/common.h"
#include "src/server.h"

namespace var {

void IndexService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/index?as_more";
    info->tab_name = "更多";
}

void IndexService::default_method(net::HttpRequest* request, net::HttpResponse* response) {
    response->header().set_content_type("text/html");
    const bool use_html = UseHTML(request->header());
    // const bool as_more = request->header().url().GetQuery("as_more");
    
    net::BufferStream os;
    if(use_html) {
        os << "<!DOCTYPE html><html>";
        // for as_more.
        os << "<head>\n"
            "<meta charset=\"UTF-8\">\n"
            "<script language=\"javascript\" type=\"text/javascript\" src=\"/js/jquery_min\"></script>\n"
            << TabsHead()
            << "</head>\n";
        os << "<body>\n";
        // for as more.
        Server* server = static_cast<Server*>(_owner);
        if(server) {
            server->PrintTabsBody(os, "more"); 
        }
        os << "<pre>";
    }
    os << logo();
    if(use_html) {
        os << "</pre>";
    }
    os << '\n';
    if(use_html) {
        os << "<a href=\"https://github.com/apache/brpc\">github</a>";
    } 
    else {
        os << "github : https://github.com/apache/brpc";
    }
    ///@todo body
    if(use_html) {
        os << "</body></html>";
    }
    net::Buffer content;
    os.moveTo(content);
    response->set_body(content);
}

} // end namespace var