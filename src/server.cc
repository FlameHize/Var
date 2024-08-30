#include "server.h"
#include "builtin/index_service.h"
#include "builtin/vars_service.h"
#include "builtin/get_js_service.h"

namespace var {

static Server* g_dummy_server = nullptr;
static Thread* g_thread = nullptr;

bool IsDummyServerRunning() {
    return g_dummy_server != nullptr;
}

bool StartDummyServerAt(int port) {
    if(port < 0 || port >= 65536) {
        LOG_ERROR << "Invalid port=" << port;
        return false;
    }
    if(!g_dummy_server) {
        g_thread = new Thread([port](){
            net::InetAddress addr(port);
            g_dummy_server = new Server(addr);
            g_dummy_server->Start();
        });
        g_thread->start();
        return true;
    }
    LOG_ERROR << "Already has dummy server";
    return false;
}

Server::Server(const net::InetAddress& addr) 
    :_server(&_loop, addr, std::string("DummyServer")) {
    _server.SetHttpCallback(std::bind(&Server::ProcessRequest, this, _1, _2));
}

Server::~Server() {
    _loop.quit();
}

void Server::Start() {
    if(!AddBuiltinService("index", new (std::nothrow) IndexService)) {
        LOG_ERROR << "Failed to add IndexService";
    }
    if(!AddBuiltinService("vars", new (std::nothrow) VarsService)) {
        LOG_ERROR << "Failed to add VarsService";
    }
    if(!AddBuiltinService("js", new (std::nothrow) GetJsService)) {
        LOG_ERROR << "Failed to add GetJsService";
    }
    
    _server.Start();
    _loop.loop();
}

bool Server::AddBuiltinService(const std::string& service_name, Service* service) {
    if(!service) {
        LOG_ERROR << "Parameter[service] is NULL";
        return false;
    }
    if(_service_map.find(service_name) != _service_map.end()) {
        LOG_WARN << "Service " << service_name << " has already added";
        return false;
    }
    _service_map[service_name] = service;
    return true;
}

const Service*
Server::FindServiceByName(const std::string& service_name) const {
    auto iter = _service_map.find(service_name);
    return iter != _service_map.end() ? iter->second : nullptr;
}

const Service::Method*
Server::FindMethodByUrl(const std::string& url_path, std::string* unresolved_path) const {
    StringSplitter splitter(url_path.c_str(), '/');
    if(!splitter) {
        const Service* index_service = FindServiceByName("index");
        const Service::Method* index_method = index_service->FindMethodByName("default-method");
        return index_method;
    }
    std::string service_name(splitter.field(), splitter.length());
    const Service* service = FindServiceByName(service_name);
    if(!service)
        return nullptr;

    const Service::Method* method = nullptr;
    std::string method_name;
    splitter++;
    // Regard URL as [service_name]/[method_name].
    if(splitter) {
        method_name.append(splitter.field(), splitter.length());
        method = service->FindMethodByName(method_name);
        if(method) {
            ++splitter;
            net::HttpServer::FillUnresolvedPath(unresolved_path, url_path, splitter);
            return method;
        }
    }

    // Try [service_name]/[default-method]
    CommonStrings common;
    method = service->FindMethodByName(common.DEFAULT_METHOD);
    if(method) {
        net::HttpServer::FillUnresolvedPath(unresolved_path, url_path, splitter);
        return method;
    }
    return nullptr;
}

void Server::ProcessRequest(net::HttpRequest* request, net::HttpResponse* response) {
    net::HttpHeader& header = request->header();
    const std::string url_path = header.url().path();
    std::string unresolved_path;
    const Service::Method* method = FindMethodByUrl(url_path, &unresolved_path);
    if(method) {
        header.set_unresolved_path(unresolved_path);
        (*method)(request, response);
    }
}

} // end namespace var