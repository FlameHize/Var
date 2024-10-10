#include "src/builtin/log_service.h"
#include "src/builtin/common.h"
#include "src/server.h"

namespace var {

bool FLAGS_enable_log = false;

LogService::LogService() {
    AddMethod("enable", std::bind(&LogService::enable,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("disable", std::bind(&LogService::disable,
                this, std::placeholders::_1, std::placeholders::_2));
}

void LogService::enable(net::HttpRequest* request,
                        net::HttpResponse* response) {
    const bool use_html = UseHTML(request->header());
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
    FLAGS_enable_log = true;
    ///@todo redirect log output func.
    net::BufferStream os;
    if(use_html) {
        // Redirect to /log
        os << "<!DOCTYPE html><html><head>"
        "<meta http-equiv=\"refresh\" content=\"0; url=/log\" />"
        "</head><body>";
    }
    os << "log is enabled</body></html>";
    response->set_body(os);
}

void LogService::disable(net::HttpRequest* request,
                         net::HttpResponse* response) {
    const bool use_html = UseHTML(request->header());
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
    FLAGS_enable_log = false;
    ///@todo redirect log output func.
    net::BufferStream os;
    if(use_html) {
        // Redirect to /log
        os << "<!DOCTYPE html><html><head>"
        "<meta http-equiv=\"refresh\" content=\"0; url=/log\" />"
        "</head><body>";
    }
    os << "log is disabled</body></html>";
    response->set_body(os);
}

void LogService::default_method(net::HttpRequest* request,
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
        server->PrintTabsBody(os, "log");
    }
    if(!FLAGS_enable_log) {
        if(use_html) {
            os << "<input type = 'button' style = 'height:50px;' "
            "onclick='location.href=\"/log/enable\";' value='enable'/> "
            "log to track recent calls with small overhead, "
            "you can turn it off at any time.";
        }
        else {
            os << "log is not enabled yet. You can turn on/off log by accessing"
            "/log/enable and /log/disable respectively";
        }
        os << "</body></html>";
        response->set_body(os);
        return;
    }
    if(use_html) {
        const char* action = (FLAGS_enable_log ? "disable" : "enable");
        os << "<div><input type='button' style = 'height:50px;' "
           << "onclick='location.href=\"/log/"
           << action << "\";' value='" << action << "' /></div>\n";
    }
    ///@todo Log

    if(use_html) {
        os << "</body></html>";
    }
    response->set_body(os);
}

void LogService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/log";
    info->tab_name = "日志";
}

} // end namespace var