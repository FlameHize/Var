#include "index_service.h"

namespace var {

void IndexService::default_method(net::HttpRequest* request, net::HttpResponse* response) {
    response->header().set_status_code(net::HTTP_STATUS_OK);
    response->header().SetHeader("Connection", "keep-alive");
    response->header().set_content_type("text/html");
    net::BufferStream os;
    os << "<!DOCTYPE html>\n";
    os << "<html lang = \"zh-CN\">\n";
    os << "<head>\n";
    os << "<title>dummy server</title>\n";
    os << "</head>\n";
    os << "<body>\n";
    os << "<h1>hello vars server!</h1>\n";
    os << "</body>\n";
    os << "</html>";
    net::Buffer content;
    os.moveTo(content);
    response->set_body(content);
}

} // end namespace var