#include "vars_service.h"

namespace var {

void VarsService::default_method(net::HttpRequest* request, net::HttpResponse* response) {
    std::string unresolved_path = request->header().unresolved_path();
    response->set_body("<h1>" + unresolved_path + "</h1>");
}

} // end namespace var