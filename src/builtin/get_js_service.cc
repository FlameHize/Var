#include "get_js_service.h"
#include "jquery_min_js.h"
#include "flot_min_js.h"

namespace var {

static const char* g_last_modified = "Wed, 16 Sep 2015 01:25:30 GMT";

static void Time2GMT(time_t t, char* buf, size_t size) {
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, size, "%a, %d %b %Y %H:%M:%S %Z", &tm);
}

static void SetExpires(net::HttpHeader* header, time_t seconds) {
    char buf[256];
    time_t now = time(0);
    Time2GMT(now, buf, sizeof(buf));
    header->SetHeader("Date", buf);
    Time2GMT(now + seconds, buf, sizeof(buf));
    header->SetHeader("Expires", buf);
}

GetJsService::GetJsService() {
    AddMethod("jquery_min", std::bind(&GetJsService::jquery_min,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("flot_min", std::bind(&GetJsService::flot_min,
                this, std::placeholders::_1, std::placeholders::_2));
}

void GetJsService::jquery_min(net::HttpRequest* request, net::HttpResponse* response) {
    response->header().SetHeader("Connection", "keep-alive");
    response->header().set_status_code(net::HTTP_STATUS_OK);
    net::HttpHeader& request_header = request->header();
    const std::string* ims = request_header.GetHeader("If-Modified-Since");
    if(ims && *ims == g_last_modified) {
        response->header().set_status_code(net::HTTP_STATUS_NOT_MODIFIED);
        return;
    }
    response->header().SetHeader("Last-Modified", g_last_modified);
    response->header().set_content_type("application/javascript");
    response->set_body(jquery_min_js_buf());
    SetExpires(&response->header(), 600);
}

void GetJsService::flot_min(net::HttpRequest* request, net::HttpResponse* response) {
    response->header().SetHeader("Connection", "keep-alive");
    response->header().set_status_code(net::HTTP_STATUS_OK);
    net::HttpHeader& request_header = request->header();
    const std::string* ims = request_header.GetHeader("If-Modified-Since");
    if(ims && *ims == g_last_modified) {
        response->header().set_status_code(net::HTTP_STATUS_NOT_MODIFIED);
        return;
    }
    response->header().SetHeader("Last-Modified", g_last_modified);
    response->header().set_content_type("application/javascript");
    response->set_body(flot_min_js_buf());
    SetExpires(&response->header(), 600);
}

} // end namespace var