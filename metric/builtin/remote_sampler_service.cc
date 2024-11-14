#include "metric/builtin/remote_sampler_service.h"
#include "metric/builtin/common.h"
#include "metric/server.h"
#include "metric/util/json.hpp"

namespace var {

RemoteSamplerService::RemoteSamplerService() {
    AddMethod("status", std::bind(&RemoteSamplerService::status,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("latency", std::bind(&RemoteSamplerService::latency,
                this, std::placeholders::_1, std::placeholders::_2));
}

void RemoteSamplerService::status(net::HttpRequest* request,
                                  net::HttpResponse* response) {
    std::string body_str = request->body().retrieveAllAsString();
    if(body_str.empty()) {
        return;
    }
    std::vector<std::string> metric_name_list;
    Variable::list_exposed(&metric_name_list);

    nlohmann::json body_json = nlohmann::json::parse(body_str);
    for(auto iter = body_json.begin(); iter != body_json.end(); ++iter) {
        const std::string& metric_name = iter.key();
        const std::string& metric_value = iter.value();
        auto it = std::find(metric_name_list.begin(), metric_name_list.end(), metric_name);
        if(it != metric_name_list.end()) {
            for(size_t i = 0; i < _status_metric_list.size(); ++i) {
                Status<int64_t>* metric = _status_metric_list.at(i);
                if(metric->name() == metric_name) {
                    metric->set_value(std::stoi(metric_value));
                    break;
                }
            }
        }
        else {
            Status<int64_t>* metric = new Status<int64_t>();
            metric->expose(metric_name);
            metric->set_value(std::stoi(metric_value));
            _status_metric_list.push_back(metric);
        }
    }
}

void RemoteSamplerService::latency(net::HttpRequest* request,
                                   net::HttpResponse* response) {
    std::string body_str = request->body().retrieveAllAsString();
    if(body_str.empty()) {
        return;
    }
    std::vector<std::string> metric_name_list;
    Variable::list_exposed(&metric_name_list);

    nlohmann::json body_json = nlohmann::json::parse(body_str);
    for(auto iter = body_json.begin(); iter != body_json.end(); ++iter) {
        const std::string& metric_name = iter.key();
        const std::string& metric_value = iter.value();
        auto it = std::find(metric_name_list.begin(), metric_name_list.end(), metric_name);
        if(it != metric_name_list.end()) {
            for(size_t i = 0; i < _latency_metric_list.size(); ++i) {
                LatencyRecorder* metric = _latency_metric_list.at(i);
                std::string metric_within_latency_name = metric_name + "_latency";
                if(metric->latency_name() == metric_within_latency_name) {
                    *metric << std::stoi(metric_value);
                    break;
                }
            }
        }
        else {
            LatencyRecorder* metric = new LatencyRecorder();
            metric->expose(metric_name);
            *metric << std::stoi(metric_value);
            _latency_metric_list.push_back(metric);
        }
    }
}

void RemoteSamplerService::default_method(net::HttpRequest* request,
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
        server->PrintTabsBody(os, "远程采样");
    }
    ///@todo user function describe.
    if(use_html) {
        os << "</pre></body></html>";
    }
    response->set_body(os);
}

void RemoteSamplerService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/remote_sampler";
    info->tab_name = "远程采样";
}

} // end namespace var