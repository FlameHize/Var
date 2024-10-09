#include "service.h"
#include "net/base/Logging.h"

namespace var {

CommonStrings::CommonStrings() 
    : DEFAULT_PATH("/")
    , DEFAULT_METHOD("default-method")
{}

Service::Service() 
    : _owner(nullptr) {
    AddMethod("default-method", std::bind(&Service::default_method,
                this, std::placeholders::_1, std::placeholders::_2));
}

Service::~Service() {
}

void Service::AddMethod(const std::string& method_name, const Method& method) {
    if(method_name.empty()) return;
    if(_method_map.find(method_name) != _method_map.end()) {
        LOG_WARN << "Method " << method_name << " has already add";
        return;
    }
    _method_map[method_name] = method;
}

const Service::Method*
Service::FindMethodByName(const std::string& method_name) const {
    auto iter = _method_map.find(method_name);
    return iter != _method_map.end() ? &iter->second : nullptr;
}

void Service::default_method(net::HttpRequest* request, net::HttpResponse* response) {
    LOG_INFO << "wait for complete this func";
}

} // end namespace var