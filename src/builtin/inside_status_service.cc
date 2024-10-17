#include "src/builtin/inside_status_service.h"
#include "src/builtin/common.h"
#include "src/server.h"
#include "src/util/dir_reader_linux.h"
#include "src/util/json.hpp"
#include "net/base/FileUtil.h"

namespace var {

static bool kInitFlag = false;
const std::string InsideStatusXMLFileSaveDir = "data/inside_status/";

InsideStatusService::InsideStatusService() {
    AddMethod("add_user", std::bind(&InsideStatusService::add_user,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("add_user_internal", std::bind(&InsideStatusService::add_user_internal,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("delete_user", std::bind(&InsideStatusService::delete_user,
                this, std::placeholders::_1, std::placeholders::_2));           
}

void InsideStatusService::add_user(net::HttpRequest* request,
                                   net::HttpResponse* response) {
    const bool use_html = UseHTML(request->header());
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
    net::BufferStream os;
    if(use_html) {
        os << "<!DOCTYPE html><html><head>\n"
           << "<meta charset=\"UTF-8\">\n"
           << "<script language=\"javascript\" type=\"text/javascript\" src=\"/js/jquery_min\"></script>\n"
           << "</head><body>";
    }

    if(use_html) {
        os << "<form id=\"user-form\" enctype=\"multipart/form-data\">\n"
        "       <label for=\"user-name\">用户名称:</label>\n"
        "       <input type=\"text\" id=\"user-name\" name=\"user-name\" required>\n"
        "       <label for=\"user-id\">板卡组起始序号:</label>\n"
        "       <input type=\"text\" id=\"user-id\" name=\"user-id\" required>\n"
        "       <label for=\"user-file\">所属XML文件:</label>\n"
        "       <input type=\"file\" id=\"user-file\" name=\"user-file\" required>\n"
        "       <button type=\"button\" onclick=\"submitForm()\">确定</button>\n"
        "</form>\n"
        "<script>\n"
        "var fileName;\n"
        "var fileContent;\n"
        "document.getElementById('user-file').addEventListener('change', function(event){\n"
        "    const file = event.target.files[0];\n"
        "    if(file) {\n"
        "    const reader = new FileReader();\n"
        "    reader.onload = function(e) {\n"
        "        fileName = file.name;\n"
        "        fileContent = e.target.result;\n"
        "    };\n"
        "    reader.readAsText(file);\n"
        "    }\n"
        "});\n"
        "function submitForm() {\n"
        "    var userName = document.getElementById('user-name').value;\n"
        "    var userId = document.getElementById('user-id').value;\n"
        "    $.ajax({\n"
        "    url: '/inside_status/add_user_internal',\n"
        "    type: \"POST\",\n"
        "    data: JSON.stringify({\n"
        "        userName: userName,\n"
        "        userId: userId,\n"
        "        fileName: fileName,\n"
        "        fileContent: fileContent\n"
        "    }),\n"
        "    processData: false,\n"
        "    contentType: false,\n"
        "    success: function(response) {\n"
        "        alert('导入用户信息成功');\n"
        "        window.location.href = '/inside_status';\n"
        "    },\n"
        "    error: function(error) {\n"
        "        alert('导入用户信息失败，请稍后重试');\n"
        "        console.log('Failed submit form', error);\n"
        "    }\n"
        "    });\n"
        "}\n"
        "</script>\n";
    }

    if(use_html) {
        os << "</body></html>";
    }
    response->set_body(os);
}

void InsideStatusService::add_user_internal(net::HttpRequest* request,
                                            net::HttpResponse* response) {
    std::string body_str = request->body().retrieveAllAsString();
    nlohmann::json body_json = nlohmann::json::parse(body_str);
    std::string user_name = body_json["userName"];
    std::string user_id = body_json["userId"];
    std::string file_name = body_json["fileName"];
    std::string file_content = body_json["fileContent"];
    
    if(file_name.empty() || file_content.empty()) {
        LOG_ERROR << "Inside status xml file format error";
        return;
    }
    
    std::string path = InsideStatusXMLFileSaveDir;
    path += (user_name + '_' + user_id);
    if(!DirReaderLinux::CreateDirectoryIfNotExists(path.c_str())) {
        LOG_ERROR << "Inside status xml file dir not existed";
        return;
    }
    if(!DirReaderLinux::ClearDirectory(path.c_str())) {
        LOG_ERROR << "Clear " << path << " old xml file failed";
        return;
    }
    std::string file_path = path + '/' + file_name;
    FileUtil::AppendFile file(file_path);
    file.append(file_content.data(), file_content.length());
    file.flush();

    InsideCmdStatusUser *user = new InsideCmdStatusUser(user_name, std::stoi(user_id));
    if(user->parse(file_path.c_str()) == 0) {
        for(auto iter = _user_list.begin(); iter != _user_list.end(); ++iter) {
            InsideCmdStatusUser* tmp = *iter;
            if(tmp->name() == user_name) {
                _user_list.erase(iter);
                delete tmp;
                tmp = nullptr;
                break;
            }
        }
        _user_list.push_back(user);
    }
    else {
        LOG_ERROR << "Parse " << user_name << "'s xml file "
                  << file_name << " failed";
    }
    return;
}

void InsideStatusService::delete_user(net::HttpRequest* request,
                                      net::HttpResponse* response) {
    const bool use_html = UseHTML(request->header());
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
    net::BufferStream os;
    if(use_html) {
        os << "<!DOCTYPE html><html><head>\n"
           << "<meta charset=\"UTF-8\">\n"
           << "<script language=\"javascript\" type=\"text/javascript\" src=\"/js/jquery_min\"></script>\n"
           << "</head><body>";
    }
    if(use_html) {
        const std::string* name = request->header().url().GetQuery("username");
        if(!name) {
            // Direct submit form data in url query.
            os << "<form id=\"form_user\" method=\"get\">\n" 
            << "<label for=\"selected-user\">选择用户</label>\n"
            << "<select name=\"username\" id=\"selected-user\" style=\"min-width:60px;\">\n";
            for(size_t i = 0; i < _user_list.size(); ++i) {
                InsideCmdStatusUser* tmp = _user_list.at(i);
                std::string display_name = tmp->name();
                os << "<option value=\"" << display_name << "\">"
                << display_name  << "</option>\n";
            }
            os << "</select>\n"
            << "<input type=\"submit\" value=\"删除\"></form>";
        }
        else {
            // Has already selected the delete user.
            std::string path = InsideStatusXMLFileSaveDir;
            if(!DirReaderLinux::DirectoryExists(path.c_str())) {
                LOG_ERROR << "Wait deleted user xml file dir" << path << "not existed";
                os << "<script>alert(Delete " << *name << " failed);</script>";
                response->set_body(os);
                return;
            }
            std::vector<string> user_dir_paths;
            if(!DirReaderLinux::ListChildDirectorys(InsideStatusXMLFileSaveDir.c_str(), 
                                                    user_dir_paths)) {
                LOG_ERROR << "List dirpath: " << InsideStatusXMLFileSaveDir
                        << "'s user dir path failed";
                os << "<script>alert(Empty  " << path << " );</script>"; 
                return;
            }
            for(size_t i = 0; i < user_dir_paths.size(); ++i) {
                const std::string& user_path = user_dir_paths.at(i);
                StringSplitter sp(user_path, '/');
                std::string user_name_id;
                for(; sp; ++sp) {
                    user_name_id = std::string(sp.field(), sp.length());
                }
                StringSplitter sp2(user_name_id, '_');
                std::string user_name(sp2.field(), sp2.length());
                if(user_name == *name) {
                    // Do not delete file memory now, because
                    // other users may using this at same time.
                    DirReaderLinux::DeleteDirectory(user_path.c_str());
                    break;
                }
            }
            os << "<script>alert('已删除用户" << *name << "');\n"
            "        window.location.href=\"/inside_status\";\n"
            "</script>\n";
        }

    }
    if(use_html) {
        os << "</body></html>";
    }
    response->set_body(os);
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
        server->PrintTabsBody(os, "内部状态");
    }

    // Just init once resource.
    if(!kInitFlag) {
        std::vector<string> user_dir_paths;
        if(!DirReaderLinux::ListChildDirectorys(InsideStatusXMLFileSaveDir.c_str(), 
                                                user_dir_paths)) {
            LOG_ERROR << "List dirpath: " << InsideStatusXMLFileSaveDir
                      << "'s user dir path failed";
            return;
        }
        // Used for read first file back equal with .xml.
        auto get_first_xml_file = [](const std::string& dir_path) -> std::string {
            if(dir_path.empty()) {
                return std::string();
            }
            DIR* dir = opendir(dir_path.c_str());
            if(!dir) {
                LOG_ERROR << "Failed to open directory: " << dir_path;
                return std::string();
            }
            struct dirent* entry;
            while((entry = readdir(dir)) != nullptr) {
                if(entry->d_type == DT_REG && entry->d_name[0] != '.') {
                    size_t name_len = strlen(entry->d_name);
                    if(name_len >= 4 && 
                        strcmp(entry->d_name + name_len - 4, ".xml") == 0) {
                        std::string file_path = dir_path + '/';
                        file_path += std::string(entry->d_name); 
                        closedir(dir);
                        return file_path;
                    }
                }
            }
            return std::string();
        };
        std::vector<std::string> user_file_paths;
        for(size_t i = 0; i < user_dir_paths.size(); ++i) {
            std::string dir_path = user_dir_paths.at(i);
            std::string file_path = get_first_xml_file(dir_path);
            if(file_path.empty()) {
                continue;
            }
            StringSplitter sp(dir_path, '/');
            std::string user_name_id;
            for(; sp; ++sp) {
                user_name_id = std::string(sp.field(), sp.length());
            }
            StringSplitter sp2(user_name_id, '_');
            std::string name(sp2.field(), sp2.length());
            ++sp2;
            std::string id(sp2.field(), sp2.length());
            InsideCmdStatusUser* user = new InsideCmdStatusUser(name, std::stoi(id));
            if(user->parse(file_path.c_str()) == 0) {
                _user_list.push_back(user);
            }
        }
        kInitFlag = true;
    }
    
    // User Layer.
    if(use_html) {
        os << "<form id=\"form_user\" method=\"get\">\n" 
           << "<label for=\"selected-user\">选择用户</label>\n"
           << "<select name=\"username\" id=\"selected-user\""
           << "style=\"min-width:60px; min-height:20px;\">\n";
        for(size_t i = 0; i < _user_list.size(); ++i) {
            InsideCmdStatusUser* tmp = _user_list.at(i);
            std::string display_name = tmp->name();
            os << "<option value=\"" << display_name << "\">"
               << display_name  << "</option>\n";
        }
        os << "</select>\n"
           << "<input type=\"submit\" value=\"确定\">\n";
        os << "<input type='button' "
           << "onclick='location.href=\"/inside_status/add_user\";' "
           << "value='添加用户'>\n";
        os << "<input type='button' "
           << "onclick='location.href=\"/inside_status/delete_user\";' "
           << "value='删除用户'>\n";
        os << "</form>\n";
    }

    ///@todo 更新选择的用户
    const std::string* user_name = request->header().url().GetQuery("username");
    if(user_name) {
        LOG_INFO << *user_name;
    }

    // os << "<script>\n"
    // "document.getElementById('fileInput').addEventListener('change', function(event) {\n"
    // "    const file = event.target.files[0];\n"
    // "    if(file) {\n"
    // "        const reader = new FileReader();\n"
    // "        reader.onload = function(e) {\n"
    // "            const content = e.target.result;\n"
    // "            $.ajax({\n"
    // "               url:  \"/inside_status/show\",\n"
    // "               type: \"POST\",\n"
    // "               data: JSON.stringify({filename: file.name, content: content}),\n"
    // "               dataType: \"html\",\n"
    // "               success: function(data) {\n"
    // "                   $('#status-content').html(data);\n"
    // "               }\n"
    // "            });\n"
    // "        };\n"
    // "        reader.readAsText(file);\n"
    // "    }\n"
    // "});\n"
    // "</script>\n";

    if(use_html) {
        os << "<pre id = \"status-content\">\n";
    }
    
    if(use_html) {
        os << "</pre></body></html>";
    }
    response->set_body(os);
}

void InsideStatusService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/inside_status";
    info->tab_name = "内部状态";
}

void ChipInfo::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/inside_status/" + label;
    info->tab_name = label;
}

} // namespace var