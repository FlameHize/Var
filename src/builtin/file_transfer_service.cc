#include "src/builtin/file_transfer_service.h"
#include "src/builtin/common.h"
#include "src/util/json.hpp"
#include "src/server.h"

namespace var {

FileTransferService::FileTransferService() {
    AddMethod("update_file", std::bind(&FileTransferService::update_file,
                this, std::placeholders::_1, std::placeholders::_2));
}

void FileTransferService::update_file(net::HttpRequest* request,
                                      net::HttpResponse* response) {
    std::string file_data = request->body().retrieveAllAsString();
    nlohmann::json file_json = nlohmann::json::parse(file_data);
    std::string file_name = file_json["fileName"];
    // std::string file_content = file_json["fileContent"];
    int file_cur_chunk = file_json["fileCurChunk"];
    int file_total_chunks = file_json["fileTotalChunks"];
    LOG_INFO << file_name;
    LOG_INFO << file_cur_chunk;
    LOG_INFO << file_total_chunks;
}

void FileTransferService::default_method(net::HttpRequest* request,
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
        server->PrintTabsBody(os, "文件传输");
    }

    // Update/Export file.
    if(use_html) {
        os << "<form id=\"file-form\" enctype=\"multipart/form-data\">\n"
        "       <label for=\"user-file\">所属XML文件:</label>\n"
        "       <input type=\"file\" id=\"user-file\" name=\"user-file\" required>\n"
        "       <button type=\"button\" onclick=\"submitForm()\">确定</button>\n"
        "</form>\n"
        "<script>\n"
        "var file;\n"
        "var fileName;\n"
        "var fileContent;\n"
        "function submitForm() {\n"
        "    file = document.getElementById('user-file').files[0];\n"
        "    fileName = file.name;\n"
        "    const CHUNK_SIZE = 1024 * 1024;\n"
        "    let offset = 0;\n"
        "    const totalChunks = Math.ceil(file.size / CHUNK_SIZE);\n"

        // "    function sendChunk(file, start, end) {\n"
        // "        const reader = new FileReader();\n"
        // "        const slice = file.slice(start, end);\n"
        // "        reader.onload = (event) => {\n"
        // "        const content = event.target.result;\n"
        // "        fetch('/file_transfer/update_file', {\n"
        // "               method: 'POST',\n"
        // "               headers: {\n"
        // "                   'Content-Type': 'application/octet-stream',\n"
        // "                   'Content-Range': 'bytes ${start}-${end - 1}/${file.size}'\n"
        // "               },\n"
        // "               body: content\n"
        // "        }).then(response =>{\n"
        // "           if(response.ok) {\n"
        // "               if(end < file.size) {\n"
        // "                   sendChunk(file, end, Math.min(end + CHUNK_SIZE, file.size));\n"
        // "               }\n"
        // "               else {\n"
        // "                   console.log('update success');\n"
        // "               }\n"
        // "           }\n"
        // "           else {\n"
        // "               console.log('update success');\n"
        // "           }\n"
        // "        });\n"
        // "    };\n"
        // "    reader.readAsArrayBuffer(slice);\n"
        // "}\n"
        // "sendChunk(file, offset, Math.min(offset + CHUNK_SIZE, file.size));\n"

        "    function sendChunk() {\n"
        "        const reader = new FileReader();\n"
        "        const slice = file.slice(offset, offset + CHUNK_SIZE);\n"
        "        reader.onload = function(e) {\n"
        "           const fileContent = event.target.result;\n"
        "           console.log(fileContent);\n"
        "           $.ajax({\n"
        "               url: '/file_transfer/update_file',\n"
        "               type: \"POST\",\n"
        "               data: JSON.stringify({\n"
        "                   fileName: fileName,\n"
        "                   fileContent: fileContent,\n"
        "                   fileCurChunk: offset / CHUNK_SIZE,\n"
        "                   fileTotalChunks: totalChunks\n"
        "                   }),\n"
        "               processData: false,\n"
        "               contentType: false,\n"
        "               success: function(response) {\n"
        "                   offset += CHUNK_SIZE;\n"
        "                   if(offset < file.size) {\n"
        "                       sendChunk();\n"
        "                   }\n"
        "                   else {\n"
        "                       console.log('update success')\n"
        "                   }\n"
        "               },\n"
        "               error: function(error) {\n"
        "                   var errStr = \"更新用户内部状态文件失败: \" + error.responseText;"
        "                   alert(errStr);\n"
        "               }\n"
        "           });\n"
        "        };\n"
        "        reader.readAsArrayBuffer(slice);\n"
        "    }\n"
        "    sendChunk();\n"
        "}\n"
        "</script>\n";
        // os << "<input type='button' style='margin-top:10px; margin-bottom:10px;'"
        //     << "onclick='location.href=\"/file_transfer\"' value='刷新文件列表'>\n";
    }

    response->set_body(os);
}

void FileTransferService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/file_transfer";
    info->tab_name = "文件传输";
}

} // end namespace var