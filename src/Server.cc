#include "Server.h"

namespace var {

const Server::ServiceProperty*
Server::FindServicePropertyByFullName(const std::string& fullname) const {
    auto iter = _fullname_service_map.find(fullname);
    return iter != _fullname_service_map.end() ? &iter->second : nullptr;
}

const Server::ServiceProperty*
Server::FindServicePropertyByName(const std::string& name) const {
    auto iter = _service_map.find(name);
    return iter != _service_map.end() ? &iter->second : nullptr;
}

const Server::MethodProperty*
Server::FindMethodPropertyByFullName(const std::string& fullname) const {
    auto iter = _method_map.find(fullname);
    return iter != _method_map.end() ? &iter->second : nullptr;
}

const Server::MethodProperty*
Server::FindMethodPropertyByFullName(const std::string& full_service_name,
                                     const std::string& method_name) const {
    const size_t fullname_len = full_service_name.size() + 1 + method_name.size();
    if(fullname_len <= 256) {
        // Avoid allocation in most cases.
        char buf[fullname_len];
        memcpy(buf, full_service_name.data(), full_service_name.size());
        buf[full_service_name.size()] = '.';
        memcpy(buf + full_service_name.size() + 1, method_name.data(), method_name.size());
        return FindMethodPropertyByFullName(std::string(buf, fullname_len));
    }
    else {
        std::string full_method_name;
        full_method_name.reserve(fullname_len);
        full_method_name.append(full_service_name.data(), full_service_name.size());
        full_method_name.push_back('.');
        full_method_name.append(method_name.data(), method_name.size());
        return FindMethodPropertyByFullName(full_method_name);
    }
}

const Server::MethodProperty*
Server::FindMethodPropertyByURL(const std::string& url_path,
                                std::string* unresolved_path) const {
    StringSplitter splitter(url_path.c_str(), '/');
    if(!splitter) {
        ///@todo Show index page for empty url.
        return nullptr;
    }
    std::string service_name(splitter.field(), splitter.length());
    const bool full_service_name = 
        (service_name.find('.') != std::string::npos);
    const Server::ServiceProperty* sp =
        (full_service_name ? 
            FindServicePropertyByFullName(service_name) :
            FindServicePropertyByName(service_name));
    if(!sp) {
        return nullptr;
    }

    if(!full_service_name) {
        ///@todo Change to service's fullname.
    }

    // Regard URL as [service_name]/[method_name].
    const Server::MethodProperty* mp = nullptr;
    std::string method_name;
    splitter++;
    if(!splitter) {
        method_name.append(splitter.field(), splitter.length());
        mp = FindMethodPropertyByFullName(service_name, method_name);
        if(mp) {
            ++splitter;
            net::HttpServer::FillUnresolvedPath(unresolved_path, url_path, splitter);
            return mp;
        }
    }

    // Try [service_name]/[default_method]
    mp = FindMethodPropertyByFullName(service_name, "default_method");
    if(mp) {
        net::HttpServer::FillUnresolvedPath(unresolved_path, url_path, splitter);
        return mp;
    }
    return nullptr;
}

} // end namespace var