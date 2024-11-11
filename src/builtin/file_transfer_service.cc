#include "src/builtin/file_transfer_service.h"
#include "src/builtin/common.h"
#include "src/server.h"
#include "src/util/dir_reader_linux.h"
#include "src/util/file_reader_linux.h"
#include "net/base/FileUtil.h"
#include "net/base/Timestamp.h"
#include <fstream>
#include <cmath>

namespace var {

void get_all_file(const std::string& dir_path, 
                  std::vector<std::string>& file_paths) {
    if(dir_path.empty()) {
        return;
    }
    DIR* dir = opendir(dir_path.c_str());
    if(!dir) {
        LOG_ERROR << "Failed to open directory: " << dir_path;
        return;
    }
    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr) {
        if(entry->d_type == DT_REG && entry->d_name[0] != '.') {
            std::string file_path = dir_path + std::string(entry->d_name); 
            file_paths.push_back(file_path);
        }
    }
}

const std::string FileTransferSaveDir = "data/file_transfer/";

FileTransferService::FileTransferService() {
    AddMethod("upload_file", std::bind(&FileTransferService::upload_file,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("download_file", std::bind(&FileTransferService::download_file,
                this, std::placeholders::_1, std::placeholders::_2));
}

void FileTransferService::upload_file(net::HttpRequest* request,
                                      net::HttpResponse* response) {
    net::BufferStream os;
    const std::string* file_cur_chunk = request->header().GetHeader("File-CurChunk");
    const std::string* file_total_chunks = request->header().GetHeader("File-TotalChunks");
    const std::string* encode_file_name = request->header().GetHeader("File-Name");
    const std::string* file_size = request->header().GetHeader("File-Size");
    if(!file_cur_chunk || !file_total_chunks || !encode_file_name || !file_size) {
        response->header().set_status_code(net::HTTP_STATUS_BAD_REQUEST);
        response->set_body(os);
        return;
    }
    net::Buffer&& content = std::move(request->body());

    std::string path = FileTransferSaveDir;
    if(!DirReaderLinux::CreateDirectoryIfNotExists(path.c_str())) {
        os << "File transfer file " << path << "dir not existed";
        response->header().set_status_code(net::HTTP_STATUS_BAD_REQUEST);
        response->set_body(os);
        return;
    }
    std::string file_name = UrlDecode(*encode_file_name);
    std::string file_path = path + file_name;
    if(std::stoi(*file_cur_chunk) == 1) {
        // Clear old data.
        FileReaderLinux file(file_path.c_str(), "w");
    }
    FileUtil::AppendFile file(file_path);
    file.append(content.peek(), content.readableBytes());
    file.flush();  
    response->header().SetHeader("Recvied-Chunk", *file_cur_chunk);
    if(*file_cur_chunk == *file_total_chunks) {
        std::string meta_path = path + "meta";
        std::ifstream meta(meta_path, std::ios::in);
        net::BufferStream meta_os;
        net::Buffer meta_info;
        if(meta) {
            std::vector<std::string> meta_lines;
            std::string line;
            while(std::getline(meta, line)) {
                meta_lines.push_back(line);
            }
            for(auto iter = meta_lines.begin(); iter != meta_lines.end(); ++iter) {
                const std::string& meta_line = *iter;
                StringSplitter sp(meta_line, ' ');
                std::string meta_name(sp.field(), sp.length());
                if(meta_name == file_name) {
                    meta_lines.erase(iter);
                    break;
                }
            }
            meta_os << file_name << " " 
                    << *file_size << " "
                    << Timestamp::now().toFormattedString(false) << " "
                    << std::to_string(0);
            meta_os.moveTo(meta_info);
            meta_lines.push_back(meta_info.retrieveAllAsString());

            std::ofstream meta_out(meta_path);
            for(size_t i = 0; i < meta_lines.size(); ++i) {
                meta_out << meta_lines.at(i) << "\n";
            }
            meta_out.close();
        }
        else {
            FileUtil::AppendFile meta_file(meta_path);
            meta_os << file_name << " " 
                    << *file_size << " "
                    << Timestamp::now().toFormattedString(false) << " "
                    << std::to_string(0);
            meta_os.moveTo(meta_info);
            std::string meta_line = meta_info.retrieveAllAsString();
            std::ofstream meta_out(meta_path);
            meta_out << meta_line << "\n";
            meta_out.close();
        }
        meta.close();
        LOG_INFO << "Success upload file: " << file_name;
    }
}

void FileTransferService::download_file(net::HttpRequest* request,
                                        net::HttpResponse* response) {
    const std::string* decode_file_name = request->header().url().GetQuery("filename");
    if(!decode_file_name) {
        LOG_ERROR << "Download file path not existed";
        return;
    }
    std::string file_name = UrlDecode(*decode_file_name);
    std::string file_path = FileTransferSaveDir + file_name;
    std::ifstream file(file_path, std::ios::binary);
    if(file) {
        response->header().set_content_type("application/octet-stream");
        response->header().SetHeader("Content-Disposition", 
        "attachment:filename=\"" + file_name + "\"; filename*=UTF-8''" + *decode_file_name);

        file.seekg(0, std::ios::end);
        size_t file_length = file.tellg();
        file.seekg(0, std::ios::beg);

        char* file_data = new char[file_length];
        file.read(file_data, file_length);
        net::BufferStream os;
        os.append(file_data, file_length);
        delete[] file_data;
        response->set_body(os);
        LOG_INFO << "Sucess download file: " << file_name;

        // Change meta log.
        std::string meta_path = FileTransferSaveDir + "meta";
        std::ifstream meta(meta_path, std::ios::in);
        net::BufferStream meta_os;
        net::Buffer meta_info;
        if(meta) {
            std::string line;
            std::vector<std::string> meta_lines;
            while(std::getline(meta, line)) {
                StringSplitter sp(line, ' ');
                std::string meta_name(sp.field(), sp.length());
                if(meta_name == file_name) {
                    sp++;
                    std::string meta_size(sp.field(), sp.length());
                    sp++;
                    std::string meta_date(sp.field(), sp.length());
                    sp++;
                    std::string meta_time(sp.field(), sp.length());
                    sp++;
                    std::string meta_count(sp.field(), sp.length());
                    int count = std::stoi(meta_count);
                    ++count;
                    meta_count = std::to_string(count);
                    std::string meta_line = meta_name + ' ' + meta_size + ' '
                        + meta_date + ' ' + meta_time + ' ' + meta_count; 
                    meta_lines.push_back(meta_line);
                }
                else {
                    meta_lines.push_back(line);
                }
            }
            std::ofstream meta_out(meta_path);
            for(size_t i = 0; i < meta_lines.size(); ++i) {
                meta_out << meta_lines.at(i) << "\n";
            }
            meta_out.close();
        }
        meta.close();
    }
    file.close();
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

    if(use_html) {
        os << "<style>\n"
        "       progress {\n"
        "           width: 100px;\n"
        "           height: 20px;\n"
        "           border: 1px solid #ccc;\n"
        "           position: relative;\n"
        "           border-radius: 10px;\n"
        "       }\n"
        "       progress::-webkit-progress-bar {\n"
        "           background-color: #f0f0f0;\n"
        "           border-radius: 10px;\n"
        "       }\n"
        "       progress::-webkit-progress-value {\n"
        "           background-color: #1bd01b;\n"
        "           border-radius: 10px;\n"
        "       }\n"
        "       progress::-moz-progress-bar {\n"
        "           background-color: #f0f0f0;\n"
        "           border-radius: 10px;\n"
        "       }\n"
        "       progress::-moz-progress-value {\n"
        "           background-color: #1bd01b;\n"
        "           border-radius: 10px;\n"
        "       }\n"
        "</style>\n";
    }

    // Update file.
    if(use_html) {
        os << "<form id=\"file-form\" enctype=\"multipart/form-data\">\n"
        "       <label for=\"user-file\">上传文件:</label>\n"
        "       <input type=\"file\" id=\"user-file\" name=\"user-file\" required>\n"
        "       <button type=\"button\" onclick=\"submitForm()\">上传</button>\n"
        "       <progress id=\"progressBar\" value=\"0\" max=\"100\"></progress>\n"
        "       <span id=\"progressText\">0%<span>\n"
        "</form>\n"
        "<script>\n"
        "var file;\n"
        "var fileName;\n"
        "var fileSize;\n"
        "function submitForm() {\n"
        "    file = document.getElementById('user-file').files[0];\n"
        "    fileName = encodeURIComponent(file.name);\n"
        "    fileSize = file.size;\n"
        "    const CHUNK_SIZE = 1024 * 1024;\n"
        "    let offset = 0;\n"
        "    let curChunk = 1;\n"
        "    const totalChunks = Math.ceil(file.size / CHUNK_SIZE);\n"

        "    function sendChunk(file, start, end) {\n"
        "        const reader = new FileReader();\n"
        "        const slice = file.slice(start, end);\n"
        "        reader.onload = (event) => {\n"
        "        const content = event.target.result;\n"
        "        fetch('/file_transfer/upload_file', {\n"
        "               method: 'POST',\n"
        "               headers: {\n"
        "                   'Content-Type': 'application/octet-stream; charset=UTF-8',\n"
        "                   'File-Name': fileName,\n"
        "                   'File-Size': fileSize,\n"
        "                   'File-CurChunk': curChunk,\n"
        "                   'File-TotalChunks': totalChunks\n"
        "               },\n"
        "               body: content\n"
        "        }).then(response =>{\n"
        "           if(response.ok) {\n"
        "               let receivedChunk = response.headers.get('Recvied-Chunk');\n"
        "               console.log(receivedChunk / totalChunks);\n"
        "               updateProgress(receivedChunk / totalChunks);\n"
        "               if(receivedChunk < totalChunks) {\n"
        "                   ++curChunk;\n"
        "                   sendChunk(file, end, Math.min(end + CHUNK_SIZE, file.size));\n"
        "               }\n"
        "               else {\n"
        "                   alert('文件上传成功');\n"
        "                   window.location.href = '/file_transfer';\n"
        "               }\n"
        "           }\n"
        "           else {\n"
        "               console.log('文件上传失败,原因: ' + response.text());\n"
        "           }\n"
        "        });\n"
        "        };\n"
        "        reader.readAsArrayBuffer(slice);\n"
        "    }\n"
        "    sendChunk(file, offset, Math.min(offset + CHUNK_SIZE, file.size));\n"
        "}\n"
        "function updateProgress(percentage) {\n"
        "    const progressBar = document.getElementById('progressBar');\n"
        "    const progressText = document.getElementById('progressText');\n"
        "    percentage *= 100;\n"
        "    if(percentage < 0) percentage = 0;\n"
        "    if(percentage > 100) percentage = 100;\n"
        "    let roundPercentage = parseFloat(percentage.toFixed(2));\n"
        "    progressBar.value = roundPercentage;\n"
        "    progressText.textContent = roundPercentage + '%';\n"
        "}\n"
        "</script>\n";
    }

    // Download file.
    if(use_html) {
        os << "<style>\n"  
        "table {\n"  
        "    width: 100%;\n"  
        "    border-collapse: collapse;\n"  
        "    margin-top: 10px;\n"
        "}\n"  
        "th, td {\n"  
        "    border: 1px solid black;\n"  
        "    padding: 8px;\n"  
        "    text-align: center;\n"  
        "    cursor: pointer;\n"  
        "}\n"  
        "th {\n"  
        "    background-color: #f2f2f2;\n"  
        "}\n"  
        "tr {\n"
        "    font-size: 14px;\n"
        "    height: 40px;\n"
        "}\n"
        "</style>\n";

        os << "<table>\n"
        "<tr>\n"
        "   <th>下载链接</th>\n"
        "   <th>文件大小</th>\n"
        "   <th>上传时间</th>\n"
        "   <th>下载次数</th>\n"
        "</tr>\n";
        std::string meta_path = FileTransferSaveDir + "meta";
        std::ifstream meta(meta_path, std::ios::in);
        if(meta) {
            std::string line;
            std::vector<std::string> meta_lines;
            while(std::getline(meta, line)) {
                meta_lines.push_back(line);
            }
            std::reverse(meta_lines.begin(), meta_lines.end());
            for(size_t i = 0; i < meta_lines.size(); ++i) {
                const std::string& tmp = meta_lines.at(i);
                StringSplitter sp(tmp, ' ');
                std::string meta_name(sp.field(), sp.length());
                sp++;
                std::string meta_size(sp.field(), sp.length());
                sp++;
                std::string meta_date(sp.field(), sp.length());
                sp++;
                std::string meta_time(sp.field(), sp.length());
                sp++;
                std::string meta_count(sp.field(), sp.length());
                os << "<tr>\n";
                os << "<td><a href=\"/file_transfer/download_file?filename="
                   << UrlDecode(meta_name)
                   << "\" download=\"" << meta_name << "\">" 
                   << meta_name << "</a></td>\n";
                
                os << "<td>" << format_byte_size(std::stoi(meta_size)) << "</td>\n";
                // os << "<td>" << meta_size << "</td>\n";
                os << "<td>" << meta_date + ' ' + meta_time << "</td>\n";
                os << "<td>" << meta_count << "</td>\n";
                os << "</tr>\n";
            }
        }
        os << "</table>\n";
        meta.close();
    }

    if(use_html) {
        os << "</body></html>\n";
    }

    response->set_body(os);
}

void FileTransferService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/file_transfer";
    info->tab_name = "文件传输";
}

} // end namespace var