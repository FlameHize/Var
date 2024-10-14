#include "src/builtin/inside_status_service.h"
#include "src/builtin/common.h"
#include "src/server.h"
#include "src/util/dir_reader_linux.h"
#include "src/util/tinyxml2.h"

using namespace tinyxml2;

namespace var {

const std::string InsideStatusXMLFileSaveDir = "data/inside_status/";

InsideStatusService::InsideStatusService() {
    std::string path = InsideStatusXMLFileSaveDir;
    if(!DirReaderLinux::CreateDirectoryIfNotExists(path.c_str())) {
        LOG_ERROR << "Inside status xml file dir not existed";
        abort();
    }

    ///@todo
    XMLDocument doc;
    std::string file_path = path + "SE内部状态1_20241010.xml";
    XMLError loadResult = doc.LoadFile(file_path.c_str());
    if(loadResult != XML_SUCCESS) {
        LOG_ERROR << "Failed to load " << file_path << ", error code: " << loadResult;
        abort();
    }
    doc.Print();
}

void InsideStatusService::default_method(net::HttpRequest* request,
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
        server->PrintTabsBody(os, "log");
    }
    if(use_html) {
        os << "<div>" << request->header().url().path() << "</div>\n";
        os << "</body></html>";
    }
    response->set_body(os);
}

void InsideStatusService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/inside_status";
    info->tab_name = "内部状态";
}

} // namespace var