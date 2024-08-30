#include "vars_service.h"

namespace var {

void VarsService::default_method(net::HttpRequest* request, net::HttpResponse* response) {
    std::string unresolved_path = request->header().unresolved_path();
    response->header().set_status_code(net::HTTP_STATUS_OK);
    response->header().SetHeader("Connection", "keep-alive");
    response->set_body("<h1>" + unresolved_path + "</h1>");
}

} // end namespace var