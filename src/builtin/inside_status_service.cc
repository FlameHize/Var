#include "src/builtin/inside_status_service.h"
#include "src/builtin/common.h"
#include "src/server.h"
#include "src/util/dir_reader_linux.h"

namespace var {

const std::string InsideStatusXMLFileSaveDir = "data/inside_status/";

InsideStatusService::InsideStatusService() {
    AddMethod("show", std::bind(&InsideStatusService::show,
                this, std::placeholders::_1, std::placeholders::_2));

    std::string path = InsideStatusXMLFileSaveDir;
    if(!DirReaderLinux::CreateDirectoryIfNotExists(path.c_str())) {
        LOG_ERROR << "Inside status xml file dir not existed";
        abort();
    }

    std::string file_path = path + "SE内部状态1_20241010.xml";
    InsideCmdStatusUser user;
    user.parse(file_path.c_str());
}

void InsideStatusService::show(net::HttpRequest* request,
                               net::HttpResponse* response) {
    LOG_INFO << request->body().readableBytes();
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
    
    ///@todo
    os << "<input type = \"file\" id = \"fileInput\", style = \"display:none;\">\n"
    "<button onclick = \"document.getElementById('fileInput').click();\">"
    "导入XML测试文件</button>\n";

    os << "<script>\n"
    "document.getElementById('fileInput').addEventListener('change', function(event) {\n"
    "    const file = event.target.files[0];\n"
    "    if(file) {\n"
    "        const reader = new FileReader();\n"
    "        reader.onload = function(e) {\n"
    "            const content = e.target.result;\n"
    "            $.ajax({\n"
    "               url:  \"/inside_status/show\",\n"
    "               type: \"POST\",\n"
    "               data: JSON.stringify({filename: file.name, content: content}),\n"
    "               dataType: \"json\"\n"
    "            });\n"
    "            console.log(file.name);\n"
    "            console.log(content);\n"
    "        };\n"
    "        reader.readAsText(file);\n"
    "    }\n"
    "});\n"
    "</script>\n";
    
    if(use_html) {
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