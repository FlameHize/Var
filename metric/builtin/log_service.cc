#include "metric/builtin/log_service.h"
#include "metric/builtin/common.h"
#include "metric/util/dir_reader_linux.h"
#include "metric/server.h"
#include <deque>

namespace var {

void PutLogLevelHeading(std::ostream& os, bool selected) {
    os << "<form action=\"/log/level\" method=\"get\">\n"
       << "<select name=\"level\" id=\"selected-level\">\n"
       << "<option value=\"trace\">Trace</option>\n"
       << "<option value=\"debug\">Debug</option>\n"
       << "<option value=\"info\" "
       << (selected ? "selected" : "") << ">Info</option>\n"
       << "<option value=\"warn\">Warn</option>\n"
       << "<option value=\"error\">Error</option>\n"
       << "</select>\n"
       << "<input type=\"submit\" value=\"change\"></form>";
}

void PutLogLevelSelect(std::ostream& os, const std::string& level) {
    os << "<script>\n"
       << "var selectedValue = \"" << level << "\";\n" 
       << "var selectElement = document.getElementById('selected-level');\n"
       << "for(var i = 0; i < selectElement.options.length; ++i) {\n"
       << "  if(selectElement.options[i].value == selectedValue) {\n"
       << "     selectElement.options[i].selected = true;\n"
       << "     break;\n"
       << "  }\n"
       << "}\n"
       << "</script>\n";
}

class LogFilter {
public:
    static void log_to_stdout(const char* msg, int len) {
        std::string tmp(msg, len);
        if(_logs.size() >= _limit) {
            size_t num = _logs.size() - _limit;
            for(size_t i = 0; i <= num; ++i) {
                _logs.pop_front();
            }
        }
        _logs.push_back(tmp);
        fwrite(msg, 1, len, stdout);
    }

    static void log_to_browser(const char* msg, int len) {
        std::string tmp(msg, len);
        if(_logs.size() >= _limit) {
            size_t num = _logs.size() - _limit;
            for(size_t i = 0; i <= num; ++i) {
                _logs.pop_front();
            }
        }
        _logs.push_back(tmp);
        if(_file) {
            _file->append(msg, len);
        }
    }

    static void list_logs(std::deque<std::string>& out) {
        out = _logs;
    }

    static void set_log_file(LogFile* file) {
        _file = file;
    }

private:
    static size_t _limit;
    static std::deque<std::string> _logs;
    static LogFile* _file;
};

size_t LogFilter::_limit = 1024;
std::deque<std::string> LogFilter::_logs;
LogFile* LogFilter::_file = nullptr;

const std::string LogFileSaveDir = "data/log/";
const size_t LogFileLimitSize = 64 * 1024 * 1024;
const size_t LogFlushTimeInterval = 3;
const size_t LogFlushCheckEveryN = 10;

bool FLAGS_enable_log = false;

LogService::LogService()  
    : _file(nullptr) {
    AddMethod("enable", std::bind(&LogService::enable,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("disable", std::bind(&LogService::disable,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("level", std::bind(&LogService::set_log_level,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("update", std::bind(&LogService::update,
                this, std::placeholders::_1, std::placeholders::_2));
    if(FLAGS_enable_log) {
        std::string path = LogFileSaveDir;
        if(!DirReaderLinux::CreateDirectoryIfNotExists(path.c_str())) {
            path.clear();
            LOG_ERROR << "Log save file dir not existed";
        }
        _file = new LogFile(path, LogFileLimitSize, 
                            true, LogFlushTimeInterval, LogFlushCheckEveryN);
        LogFilter::set_log_file(_file);
        Logger::setOutput(&LogFilter::log_to_browser);
    }
    else {
        Logger::setOutput(&LogFilter::log_to_stdout);
    }
}

LogService::~LogService() {
    if(_file) {
        delete _file;
        _file = nullptr;
    }
}

void LogService::enable(net::HttpRequest* request,
                        net::HttpResponse* response) {
    const bool use_html = UseHTML(request->header());
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
    FLAGS_enable_log = true;
    if(!_file) {
        std::string path = LogFileSaveDir;
        if(!DirReaderLinux::CreateDirectoryIfNotExists(path.c_str())) {
            path.clear();
            LOG_ERROR << "Log save file dir created failed";
        }
        _file = new LogFile(path, LogFileLimitSize, 
                            true, LogFlushTimeInterval, LogFlushCheckEveryN);
        // Supply logs before enable.
        std::deque<std::string> logs;
        LogFilter::list_logs(logs);
        for(size_t i = 0; i < logs.size(); ++i) {
            std::string& log = logs.at(i);
            _file->append(log.data(), log.length());
        } 
        LogFilter::set_log_file(_file);
    }
    Logger::setOutput(&LogFilter::log_to_browser);
    net::BufferStream os;
    if(use_html) {
        // Redirect to /log
        os << "<!DOCTYPE html><html><head>"
        "<meta http-equiv=\"refresh\" content=\"0; url=/log\" />"
        "</head><body>"
        "<pre>log is enabled</pre></body></html>";
    }
    response->set_body(os);
}

void LogService::disable(net::HttpRequest* request,
                         net::HttpResponse* response) {
    const bool use_html = UseHTML(request->header());
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
    FLAGS_enable_log = false;
    Logger::setOutput(&LogFilter::log_to_stdout);
    net::BufferStream os;
    if(use_html) {
        // Redirect to /log
        os << "<!DOCTYPE html><html><head>"
        "<meta http-equiv=\"refresh\" content=\"0; url=/log\" />"
        "</head><body>"
        "<pre>log is disabled</pre></body></html>";
    }
    response->set_body(os);
}

void LogService::set_log_level(net::HttpRequest* request,
                               net::HttpResponse* response) {
    const bool use_html = UseHTML(request->header());
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
    const std::string* level = request->header().url().GetQuery("level");
    net::BufferStream os;
    if(!level) {
        os << "log level setting failed</body></html>";
    }
    else if(*level == "trace") {
        Logger::setLogLevel(Logger::TRACE);
    }
    else if(*level == "debug") {
        Logger::setLogLevel(Logger::DEBUG);
    }
    else if(*level == "info") {
        Logger::setLogLevel(Logger::INFO);
    }
    else if(*level == "warn") {
        Logger::setLogLevel(Logger::WARN);
    }
    else if(*level == "error") {
        Logger::setLogLevel(Logger::ERROR);
    }
    else {
        Logger::setLogLevel(Logger::INFO);
    }
    if(use_html) {
        // Redirect to /log
        os << "<!DOCTYPE html><html><head>"
        "<meta http-equiv=\"refresh\" content=\"0; url=/log\" />"
        "</head><body>"
        "<pre>log level set success</pre></body></html>";
    }
    response->set_body(os);
}

void LogService::update(net::HttpRequest* request,
                        net::HttpResponse* response) {
    net::BufferStream os;
    std::deque<std::string> logs;
    LogFilter::list_logs(logs);
    if(logs.empty()) {
        os << "Failed find any logs";
    }
    else {
        while(logs.size()) {
            os << logs.back();
            logs.pop_back();
        }
    }
    response->set_body(os);
}

void LogService::default_method(net::HttpRequest* request,
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
        server->PrintTabsBody(os, "日志");
    }
    if(!FLAGS_enable_log) {
        if(use_html) {
            os << "<input type = 'button'"
            "onclick='location.href=\"/log/enable\";' value='enable'/> "
            "log to track recent calls with small overhead, "
            "you can turn it off at any time.";
        }
        else {
            os << "log is not enabled yet. You can turn on/off log by accessing"
            "/log/enable and /log/disable respectively";
        }
        if(use_html) {
            os << "</body></html>";
        }
        response->set_body(os);
        return;
    }
    if(use_html) {
        const char* action = (FLAGS_enable_log ? "disable" : "enable");
        os << "<div><input type='button' "
           << "onclick='location.href=\"/log/"
           << action << "\";' value='" << action << "' /></div>\n";
    }
    if(use_html) {
        os << "<br></br>\n";
        PutLogLevelHeading(os, false);
        std::string level;
        switch(Logger::logLevel()) {
        case Logger::TRACE:
            level = "trace";
            break;
        case Logger::DEBUG:
            level = "debug";
            break;
        case Logger::INFO:
            level = "info";
            break;
        case Logger::WARN:
            level = "warn";
            break;
        case Logger::ERROR:
            level = "error";
            break;
        default:
            level = "info";
            break;
        }
        PutLogLevelSelect(os, level);
    }

    if(use_html) {
        os << "<script>\n"
            "function fetchData() {\n"
                "$.ajax({\n"
                "url: \"/log/update\",\n"
                "type: \"GET\",\n"
                "dataType: \"html\",\n"
                "success: function(data) {\n"
                "    $('#logs-content').html(data);\n"
                "}\n"
            "});\n"
        "}\n"
        "setInterval(fetchData, 1000);\n"
        "</script>\n";
    }
    else {
        std::deque<std::string> logs;
        LogFilter::list_logs(logs);
        if(logs.empty()) {
            os << "Failed find any logs";
        }
        else {
            while(logs.size()) {
                os << logs.back();
                logs.pop_back();
            }
        }
    }

    if(use_html) {
        os << "<pre id = \"logs-content\">\n";
    }

    if(use_html) {
        os << "</pre></body></html>";
    }
    response->set_body(os);
}

void LogService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/log";
    info->tab_name = "日志";
}

} // end namespace var