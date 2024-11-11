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

Service::Method* Service::FindMethodByName(const std::string& method_name) const {
    MethodMap::const_iterator iter = _method_map.find(method_name);
    if(iter == _method_map.end()) {
        return nullptr;
    }
    return const_cast<Method*>(&iter->second);
}

void Service::default_method(net::HttpRequest* request, net::HttpResponse* response) {
    LOG_INFO << "wait for complete this func";
}

} // end namespace var