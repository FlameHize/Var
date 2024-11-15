#include "metric/server.h"
#include "metric/builtin/get_js_service.h"
#include "metric/builtin/index_service.h"
#include "metric/builtin/vars_service.h"
#include "metric/builtin/log_service.h"
#include "metric/builtin/inside_status_service.h"
#include "metric/builtin/inside_cmd_service.h"
#include "metric/builtin/xvc_service.h"
#include "metric/builtin/file_transfer_service.h"
#include "metric/builtin/remote_sampler_service.h"
#include "metric/builtin/profiler_service.h"

namespace var {

static Server* g_dummy_server = nullptr;
static Thread* g_thread = nullptr;
static InsideCmdCallback g_inside_cmd_callback;

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
        }, "dummy_server");
        g_thread->start();
        return true;
    }
    LOG_ERROR << "Already has dummy server";
    return false;
}

void UpdateInsideStatusData(const char* data, size_t len) {
    if(!IsDummyServerRunning()) {
        LOG_ERROR << "Dummy server has not running";
        return;
    }
    Service* service = g_dummy_server->FindServiceByName("inside_status");
    if(!service) {
        LOG_ERROR << "Inside status service has not registered";
        return;
    }
    static_cast<InsideStatusService*>(service)->SetData(data, len);
}

void RegisterInsideCmdCallback(const InsideCmdCallback& cb) {
    g_inside_cmd_callback = cb;
}

Server::Server(const net::InetAddress& addr) 
    : _addr(addr)
    , _server(&_loop, _addr, std::string("DummyServer"))
    , _tab_info_list(nullptr) {
    _server.SetHttpCallback(std::bind(&Server::ProcessRequest, this, _1, _2));
}

Server::~Server() {
    _loop.quit();
}

void Server::Start() {
    if(!AddBuiltinService("js", new (std::nothrow) GetJsService)) {
        LOG_ERROR << "Failed to add GetJsService";
    }
    if(!AddBuiltinService("vars", new (std::nothrow) VarsService)) {
        LOG_ERROR << "Failed to add VarsService";
    }
    if(!AddBuiltinService("log", new(std::nothrow) LogService)) {
        LOG_ERROR << "Failed to add LogService";
    }
    if(!AddBuiltinService("inside_cmd", new(std::nothrow) InsideCmdService)) {
        LOG_ERROR << "Failed to add InsideCmdService";
    }
    if(!AddBuiltinService("inside_status", new(std::nothrow) InsideStatusService)) {
        LOG_ERROR << "Failed to add InsideStatusService";
    }
    if(!AddBuiltinService("xvc", new(std::nothrow) XvcService)) {
        LOG_ERROR << "Failed to add XvcService";
    }
    if(!AddBuiltinService("profiler", new(std::nothrow) ProfilerService)) {
        LOG_ERROR << "Failed to add ProfilerService";
    }
    if(!AddBuiltinService("file_transfer", new(std::nothrow) FileTransferService)) {
        LOG_ERROR << "Failed to add FileTransferService";
    }
    if(!AddBuiltinService("remote_sampler", new (std::nothrow) RemoteSamplerService)) {
        LOG_ERROR << "Failed to add RemoteSamplerService";
    }
    if(!AddBuiltinService("index", new (std::nothrow) IndexService)) {
        LOG_ERROR << "Failed to add IndexService";
    }

    if(g_inside_cmd_callback) {
        Service* service = g_dummy_server->FindServiceByName("inside_cmd");
        if(!service) {
            LOG_ERROR << "Inside cmd service has not registered";
        }
        else {
            static_cast<InsideCmdService*>(service)
                ->register_cmd_recv_callback(g_inside_cmd_callback);
        }
    }
    
    LOG_INFO << "Server is serving on port: " << _addr.port();
    LOG_INFO << "Check out http://" << _addr.toIpPort() << " in web browser";
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
    service->_name = service_name;
    service->_owner = this;
    _service_map[service_name] = service;

    // tabbed.
    // must called with -rtti.
    // Tabbed* tabbed = dynamic_cast<Tabbed*>(service);
    Tabbed* tabbed = (Tabbed*)(service);
    if(tabbed) {
        if(!_tab_info_list) {
            _tab_info_list = new TabInfoList;
        }
        const size_t last_size = _tab_info_list->size();
        tabbed->GetTabInfo(_tab_info_list);
        const size_t cur_size = _tab_info_list->size();
        for(size_t i = last_size; i != cur_size; ++i) {
            const TabInfo& info = (*_tab_info_list)[i];
            if(!info.valid()) {
                LOG_ERROR << "Invalid Tabinfo: path = " << info.path
                          << " tab_name = " << info.tab_name;
                _tab_info_list->resize(last_size);
                RemoveService(service);
                return false;
            }
        }
    }
    return true;
}

bool Server::RemoveService(Service* service) {
    if(!service) {
        LOG_ERROR << "Parameter[service] is NULL";
        return false;
    }
    const std::string& service_name = service->_name;
    auto iter = _service_map.find(service_name);
    if(iter == _service_map.end()) {
        LOG_ERROR << "Failed to remove service[" << service_name << "]";
        return false;
    }
    _service_map.erase(service_name);
    return true;
}

Service* Server::FindServiceByName(const std::string& service_name) const {
    auto iter = _service_map.find(service_name);
    return iter != _service_map.end() ? iter->second : nullptr;
}

Service::Method* Server::FindMethodByUrl(const std::string& url_path, std::string* unresolved_path) const {
    StringSplitter splitter(url_path.c_str(), '/');
    if(!splitter) {
        Service* index_service = FindServiceByName("index");
        Service::Method* index_method = index_service->FindMethodByName("default-method");
        return index_method;
    }
    std::string service_name(splitter.field(), splitter.length());
    const Service* service = FindServiceByName(service_name);
    if(!service)
        return nullptr;

    Service::Method* method = nullptr;
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

inline void tabs_li(std::ostream& os, const char* link,
                    const char* tab_name, const char* current_tab_name) {
    os << "<li id='" << link << '\'';
    if(strcmp(current_tab_name, tab_name) == 0) {
        os << " class='current'";
    }
    os << '>' << tab_name << "</li>\n";
}

void Server::PrintTabsBody(std::ostream& os, 
                           const char* current_tab_name) const {
    os << "<ul class='tabs-menu'>\n";
    if(_tab_info_list) {
        for(size_t i = 0; i < _tab_info_list->size(); ++i) {
            const TabInfo& info = (*_tab_info_list)[i];
            tabs_li(os, info.path.c_str(), info.tab_name.c_str(), current_tab_name);
        }
    }
    os << "<li id='https://github.com/apache/brpc/blob/master/docs/cn/builtin_service.md' "
        "class='help'>?</li>\n</ul>\n"
        "<div style='height:40px;'></div>";  // placeholder
}

} // end namespace var