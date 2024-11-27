#include "metric/builtin/index_service.h"
#include "metric/builtin/vars_service.h"
#include "metric/builtin/common.h"
#include "metric/server.h"

namespace var {

struct Path {
    Path(const std::string& url_str, const std::string& html_addr_str)
        : url(url_str), html_addr(html_addr_str) {}
    const std::string url;
    const std::string html_addr;
};

std::ostream& operator<<(std::ostream& os, const Path& link) {
    os << "<a href=\"http://" << link.html_addr << link.url << "\">"
       << link.url << "</a>";
    return os;
}

void IndexService::default_method(net::HttpRequest* request, net::HttpResponse* response) {
    const Server* server = static_cast<Server*>(_owner);
    const bool use_html = UseHTML(request->header());
    const bool as_more = request->header().url().GetQuery("as_more");
    if(use_html && !as_more) {
        Service* svc = server->FindServiceByName("vars");
        if(!svc) {
            LOG_ERROR << "Failed to find VarsService";
            return;
        }
        return svc->default_method(request, response);
    }
    
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
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
        if(server) {
            server->PrintTabsBody(os, "更多"); 
        }
        // os << "<pre>";
    }
    // os << logo();
    // if(use_html) {
    //     os << "</pre>";
    // }
    // os << '\n';
    // if(use_html) {
    //     os << "<a href=\"https://github.com/apache/brpc\">github</a>";
    // } 
    // else {
    //     os << "github : https://github.com/apache/brpc";
    // }

    if(use_html) {
        std::string html_addr = request->header().url().host()
            + ":" + std::to_string(request->header().url().port());
        os << "<div>" << Path("/memory", html_addr) 
           << " : Get malloc allocator information" << "</div>\n";
    }

    if(use_html) {
        os << "</body></html>";
    }
    response->set_body(os);
}

void IndexService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/index?as_more";
    info->tab_name = "更多";
}

} // end namespace var