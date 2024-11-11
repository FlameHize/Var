#include "metric/builtin/inside_status_service.h"
#include "metric/builtin/common.h"
#include "metric/server.h"
#include "metric/util/dir_reader_linux.h"
#include "metric/util/json.hpp"
#include "net/base/FileUtil.h"
#include <fstream>

namespace var {

static bool kInitFlag = false;
const std::string InsideStatusXMLFileSaveDir = "data/inside_status/";

InsideStatusService::InsideStatusService() 
    : _data(std::make_shared<net::Buffer>()) {
    AddMethod("add_user", std::bind(&InsideStatusService::add_user,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("add_user_internal", std::bind(&InsideStatusService::add_user_internal,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("delete_user", std::bind(&InsideStatusService::delete_user,
                this, std::placeholders::_1, std::placeholders::_2)); 
    AddMethod("update_file", std::bind(&InsideStatusService::update_file,
                this, std::placeholders::_1, std::placeholders::_2)); 
    AddMethod("export_file", std::bind(&InsideStatusService::export_file,
                this, std::placeholders::_1, std::placeholders::_2));   
    AddMethod("download_file", std::bind(&InsideStatusService::download_file,
                this, std::placeholders::_1, std::placeholders::_2));       
    AddMethod("show_chip_info", std::bind(&InsideStatusService::show_chip_info,
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
        "       <input type=\"text\" id=\"user-name\" name=\"user-name\" placeholder=\"请输入英文字符\">\n"
        "       <label for=\"user-id\">板卡组起始序号:</label>\n"
        "       <input type=\"text\" id=\"user-id\" name=\"user-id\" placeholder=\"请输入数字\">\n"
        "       <label for=\"user-file\">所属内部状态XML文件:</label>\n"
        "       <input type=\"file\" id=\"user-file\" name=\"user-file\">\n"
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
        "    if(userName.trim() == '') {\n"
        "        alert('用户名为空，请输入');\n"
        "        return;\n"
        "    }\n"
        "    if(userId.trim() == '') {\n"
        "        alert('用户板卡组起始序号为空，请输入');\n"
        "        return;\n"
        "    }\n"
        "    if(fileName == null) {\n"
        "        alert('用户未选择所属内部状态XML文件，请选择');\n"
        "        return;\n"
        "    }\n"
        "    if(fileContent.trim() == '') {\n"
        "        alert('用户选择文件内容为空，请重新选择');\n"
        "        return;\n"
        "    }\n"
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
        "        var errStr = \"导入用户信息失败: \" + error.responseText;"
        "        alert(errStr);\n"
        "    }\n"
        "    });\n"
        "}\n"
        "document.getElementById('user-name').addEventListener('input', function(event) {\n"
        "   var value = event.target.value;\n"
        "   var regex = /^[A-Za-z0-9]*$/;\n"
        "   if(!regex.test(value)) {\n"
        "       event.preventDefault();\n"
        "       event.target.value = value.slice(0,-1);\n"
        "   }\n"
        "   this.value = this.value.replace(/[\u4e00-\u9fa5]/g, '');\n"
        "});\n"
        "document.getElementById('user-id').addEventListener('input', function(event) {\n"
        "   var value = event.target.value;\n"
        "   var regex = /^[0-9]*$/;\n"
        "   if(!regex.test(value)) {\n"
        "       event.preventDefault();\n"
        "       event.target.value = value.slice(0,-1);\n"
        "   }\n"
        "   this.value = this.value.replace(/[\u4e00-\u9fa5]/g, '');\n"
        "});\n"
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
    
    net::BufferStream os;
    if(file_name.empty() || file_content.empty()) {
        os << "Upload xml file is empty";
        response->header().set_status_code(net::HTTP_STATUS_BAD_REQUEST);
        response->set_body(os);
        return;
    }

    InsideCmdStatusUser *user = new InsideCmdStatusUser(user_name, std::stoi(user_id));
    if(user->parse(file_content.data(), file_content.length()) != 0) {
        delete user;
        user = nullptr;
        os << "User:" << user_name << "'s xml file " 
           << file_name <<  " format is incorrect";
        response->header().set_status_code(net::HTTP_STATUS_BAD_REQUEST);
        response->set_body(os);
        return;
    }
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
    
    std::string path = InsideStatusXMLFileSaveDir;
    path += (user_name + '_' + user_id);
    if(!DirReaderLinux::CreateDirectoryIfNotExists(path.c_str())) {
        os << "Inside status xml file" << path << "dir not existed";
        response->header().set_status_code(net::HTTP_STATUS_BAD_REQUEST);
        response->set_body(os);
        return;
    }
    if(!DirReaderLinux::ClearDirectory(path.c_str())) {
        os << "Clear " << path << " old xml file failed";
        response->header().set_status_code(net::HTTP_STATUS_BAD_REQUEST);
        response->set_body(os);
        return;
    }
    std::string file_path = path + '/' + file_name;
    FileUtil::AppendFile file(file_path);
    file.append(file_content.data(), file_content.length());
    file.flush();
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
            std::vector<std::string> user_dir_paths;
            if(!DirReaderLinux::ListChildDirectorys(InsideStatusXMLFileSaveDir.c_str(), 
                                                    user_dir_paths)) {
                LOG_ERROR << "List dirpath: " << InsideStatusXMLFileSaveDir
                        << "'s user dir path failed";
                os << "<script>alert(Empty  " << path << " );</script>";
                response->set_body(os); 
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
                    DirReaderLinux::DeleteDirectory(user_path.c_str());
                    break;
                }
            }
            for(auto iter = _user_list.begin(); iter != _user_list.end(); ++iter) {
                InsideCmdStatusUser* tmp = *iter;
                if(tmp->name() == *name) {
                    _user_list.erase(iter);
                    delete tmp;
                    tmp = nullptr;
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

void InsideStatusService::update_file(net::HttpRequest* request,
                                      net::HttpResponse* response) {
    const bool use_html = UseHTML(request->header());
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
    const std::string* name = request->header().url().GetQuery("username");
    if(!name) {
        LOG_ERROR << "url query username is empty";
        return;
    }
    std::string id;
    for(size_t i = 0; i < _user_list.size(); ++i) {
        const InsideCmdStatusUser* user = _user_list.at(i);
        std::string user_name = user->name();
        if(user_name == *name) {
            id = std::to_string(user->id());
            break;
        }
    }

    net::BufferStream os;
    if(use_html) {
        os << "<!DOCTYPE html><html><head>\n"
           << "<meta charset=\"UTF-8\">\n"
           << "<script language=\"javascript\" type=\"text/javascript\" src=\"/js/jquery_min\"></script>\n"
           << "</head><body>";
    }

    if(use_html) {
        os << "<form id=\"user-form\" enctype=\"multipart/form-data\">\n"
        "       <label for=\"user-file\">所属内部状态XML文件:</label>\n"
        "       <input type=\"file\" id=\"user-file\" name=\"user-file\" required>\n"
        "       <button type=\"button\" onclick=\"submitForm()\">确定</button>\n"
        "</form>\n"
        "<script>\n"
        "var fileName;\n"
        "var fileContent;\n"
        "document.getElementById('user-file').addEventListener('change', function(event){\n"
        "    const file = event.target.files[0];\n"
        "    if(file) {\n"
        "        const reader = new FileReader();\n"
        "        reader.onload = function(e) {\n"
        "           fileName = file.name;\n"
        "           fileContent = e.target.result;\n"
        "        };\n"
        "        reader.readAsText(file);\n"
        "    }\n"
        "});\n"
        "function submitForm() {\n"
        "    if(fileName == null) {\n"
        "        alert('用户未选择所属内部状态XML文件，请选择');\n"
        "        return;\n"
        "    }\n"
        "    if(fileContent.trim() == '') {\n"
        "        alert('用户选择文件内容为空，请重新选择');\n"
        "        return;\n"
        "    }\n"
        "    $.ajax({\n"
        "    url: '/inside_status/add_user_internal',\n"
        "    type: \"POST\",\n"
        "    data: JSON.stringify({\n"
        "        userName: \"" << *name << "\",\n"
        "        userId: \"" << id << "\",\n"
        "        fileName: fileName,\n"
        "        fileContent: fileContent\n"
        "    }),\n"
        "    processData: false,\n"
        "    contentType: false,\n"
        "    success: function(response) {\n"
        "        alert('更新用户内部状态文件成功');\n"
        "        window.location.href = '/inside_status';\n"
        "    },\n"
        "    error: function(error) {\n"
        "        var errStr = \"更新用户内部状态文件失败: \" + error.responseText;"
        "        alert(errStr);\n"
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

void InsideStatusService::export_file(net::HttpRequest* request,
                                      net::HttpResponse* response) {
    const std::string* name = request->header().url().GetQuery("username");
    net::BufferStream os;
    os << "<!DOCTYPE html><html><head>\n"
       << "<meta charset=\"UTF-8\">\n"
       << "<script language=\"javascript\" type=\"text/javascript\" src=\"/js/jquery_min\"></script>\n"
       << "</head><body>";

    std::string path = InsideStatusXMLFileSaveDir;
    std::vector<std::string> user_dir_paths;
    if(!DirReaderLinux::ListChildDirectorys(path.c_str(), user_dir_paths)) {
        LOG_ERROR << "Inside_status dir is empty";
        os << "<script>alert(Empty  " << path << " );</script>"; 
        response->set_body(os);
        return;
    }
    std::string user_dir_path;
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
            user_dir_path = user_path;
            break;
        }
    }
    if(user_dir_path.empty()) {
        LOG_ERROR << *name << " user's dir not found";
        os << "<script>alert(\"" << *name << "user's dir not found\");</script>";
        response->set_body(os);
        return; 
    }
    std::string file_path = get_first_xml_file(user_dir_path);
    if(file_path.empty()) {
        LOG_ERROR << *name << " user's inside status file not found";
        os << "<script>alert(\"" << *name << "user's file not found\");</script>";
        response->set_body(os);
        return; 
    }
    StringSplitter sp(file_path, '/');
    std::string file_name;
    for(; sp; ++sp) {
        file_name = std::string(sp.field(), sp.length());
    }
    os << "<a href=\"/inside_status/download_file?userdir=" << user_dir_path << "\" download=\"" 
       << file_name << "\">下载链接: " << file_name << "</a>\n";
    os << "</body></html>";
    response->set_body(os); 
}

void InsideStatusService::download_file(net::HttpRequest* request,
                                        net::HttpResponse* response) {
    const std::string* user_dir_path =  request->header().url().GetQuery("userdir");
    if(!user_dir_path) {
        LOG_ERROR << "Download User's dir not existed";
        return;
    }
    std::string file_path = get_first_xml_file(*user_dir_path);
    StringSplitter sp(file_path, '/');
    std::string file_name;
    for(; sp; ++sp) {
        file_name = std::string(sp.field(), sp.length());
    }
    std::ifstream file(file_path, std::ios::in);
    if(file) {
        net::BufferStream os;
        response->header().set_content_type("text/plain");
        std::string encoded_filename = UrlEncode(file_name);
        response->header().SetHeader("Content-Disposition", 
        "attachment:filename=\"" + file_name + "\"; filename*=UTF-8''" + encoded_filename);
        std::string line;
        while(std::getline(file, line)) {
            os << line;
            os << "\n";
        }
        response->set_body(os);
    }
}

void InsideStatusService::show_chip_info(net::HttpRequest* request,
                                         net::HttpResponse* response) {
    const bool use_html = UseHTML(request->header());
    response->header().set_content_type(use_html ? "text/html" : "text/plain");
    const std::string* chip_index = request->header().url().GetQuery("chipIndex");
    net::BufferStream os;
    if(!chip_index) {
        return;
    }
    ChipInfo* chip = nullptr;
    for(size_t i = 0; i < _user_list.size(); ++i) {
        const InsideCmdStatusUser* user = _user_list.at(i);
        const std::vector<ChipInfo*>& chip_group = user->chip_group();
        for(size_t j = 0; j < chip_group.size(); ++j) {
            ChipInfo* tmp = chip_group.at(j);
            if(static_cast<int>(tmp->index) == std::stoi(*chip_index)) {
                chip = tmp;
                break;
            }
        }
    }
    if(!chip) {
        os << "<script>alert('未找到当前板卡信息')</script>";
        response->set_body(os);
        return;
    }
    BufPtr buf = GetData();
    chip->describe(buf->peek(), buf->readableBytes(), os, false);
    response->set_body(os);
}

void InsideStatusService::default_method(net::HttpRequest* request,
                                         net::HttpResponse* response) {
    // Just init once resource.
    if(!kInitFlag) {
        if(!DirReaderLinux::CreateDirectoryIfNotExists(InsideStatusXMLFileSaveDir.c_str())) {
            LOG_ERROR << "Inside status xml file" << InsideStatusXMLFileSaveDir 
                      << "dir not existed";
            return;
        }
        std::vector<string> user_dir_paths;
        if(!DirReaderLinux::ListChildDirectorys(InsideStatusXMLFileSaveDir.c_str(), 
                                                user_dir_paths)) {
            LOG_ERROR << "List dirpath: " << InsideStatusXMLFileSaveDir
                      << "'s user dir path failed";
            return;
        }
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
    
    // Tabs head layer.
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

    // User operation layer.
    const std::string* user_name = request->header().url().GetQuery("username");
    if(use_html) {
        os << "<form id=\"form_user\" method=\"get\">\n" 
           << "<label for=\"selected-user\">选择用户</label>\n"
           << "<select name=\"username\" id=\"selected-user\""
           << "style=\"min-width:60px; min-height:20px;\">\n";
        for(size_t i = 0; i < _user_list.size(); ++i) {
            InsideCmdStatusUser* tmp = _user_list.at(i);
            std::string display_name = tmp->name();
            if(user_name != nullptr && *user_name == display_name) {
                os << "<option value=\"" << display_name << "\" selected>"
                   << display_name  << "</option>\n";
            }
            else {
                os << "<option value=\"" << display_name << "\">"
                   << display_name  << "</option>\n";
            }
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

    if(!user_name) {
        response->set_body(os);
        return;
    }
    InsideCmdStatusUser* user = nullptr;
    for(size_t i = 0; i < _user_list.size(); ++i) {
        InsideCmdStatusUser* tmp = _user_list.at(i);
        if(tmp->name() == *user_name) {
            user = tmp;
            break;
        }
    }
    if(!user) {
        os << "<script>alert('user_name" << *user_name << "not found');</script>";
        response->set_body(os);
        return;
    }

    // Update/Export file.
    if(use_html) {
        os << "<input type='button' style='margin-top:10px; margin-bottom:10px;'"
            << "onclick='location.href=\"/inside_status/update_file"
            << "?username=" << *user_name << "\";'"
            << "value='更新文件'>\n";
        os << "<input type='button' style='margin-top:10px; margin-bottom:10px;'"
            << "onclick='location.href=\"/inside_status/export_file"
            << "?username=" << *user_name << "\";'"
            << "value='导出文件'>\n";
    }
    
    // Resolved user's all chip info internal.
    BufPtr buf = GetData();
    user->describe(buf->peek(), buf->readableBytes(), os, false);
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

void InsideStatusService::SetData(const char* data, size_t len) {
    std::lock_guard lock(_mutex);
    if(!_data.unique()) {
        BufPtr new_data(new net::Buffer(*_data));
        _data.swap(new_data);
    }
    _data->retrieveAll();
    _data->append(data, len);
}

InsideStatusService::BufPtr InsideStatusService::GetData() {
    std::lock_guard lock(_mutex);
    return _data;
}

} // namespace var