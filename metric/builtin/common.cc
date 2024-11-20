#include "metric/builtin/common.h"
#include "metric/util/cmd_reader_linux.h"
#include "net/base/StringSplitter.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <limits.h>
#include <unistd.h>

namespace var {

int hexCharToInt(char c) {  
    if (std::isdigit(c)) {  
        return c - '0';  
    } else if (std::isxdigit(c)) {  
        return std::tolower(c) - 'a' + 10;  
    } else {  
        throw std::invalid_argument("Invalid hex character");  
    }  
}  

bool UseHTML(const net::HttpHeader& header) {
    const std::string* console = header.url().GetQuery(CONSOLE_STR);
    if (console != NULL) {
        return atoi(console->c_str()) == 0;
    }
    // [curl header]
    // User-Agent: curl/7.12.1 (x86_64-redhat-linux-gnu) libcurl/7.12.1 ...
    const std::string* agent = header.GetHeader(USER_AGENT_STR);
    if (agent == NULL) {  // use text when user-agent is absent
        return false;
    }
    return agent->find("curl/") == std::string::npos;
}

std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    for(char c : value) {
        if(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        }
        else {
            escaped << '%' << std::setw(2) << std::setfill('0')
                    << std::hex << (int)(unsigned char)(c);
        }
    }
    return escaped.str();
}

std::string UrlDecode(const std::string& encode_str) {
    std::ostringstream oss;  
    std::string::const_iterator it = encode_str.cbegin();  
    while (it != encode_str.cend()) {  
        if (*it == '%' && std::next(it) != encode_str.cend() && std::next(std::next(it)) != encode_str.cend()) {   
            char hex[3] = { *(it + 1), *(it + 2), '\0' };  
            unsigned char byte = static_cast<unsigned char>((hexCharToInt(hex[0]) << 4) | hexCharToInt(hex[1]));  
            oss << byte;  
            it += 2; 
        } else if (*it == '+') {   
            oss << ' ';  
        } else {  
            oss << *it;  
        }  
        ++it;  
    }  
    return oss.str();  
}

std::string double_to_string(double value, int decimal) {
    std::ostringstream out;
    out << std::fixed;
    out << std::setprecision(decimal);
    out << value;
    return out.str();
}

std::string decimal_to_hex(int decimal) {
    std::ostringstream out;
    out << std::hex;
    out << decimal;
    return out.str();
}

std::string decimal_to_binary(int decimal) {
    if(decimal == 0) {
        return "0";
    }
    std::ostringstream out;
    while(decimal > 0) {
        out << (decimal % 2);
        decimal /= 2;
    }
    std::string binary_str = out.str();
    std::reverse(binary_str.begin(), binary_str.end());
    return binary_str;
}

std::string format_byte_size(size_t value) {
    std::string str;
    if(value < 1024) {
        str = std::to_string(value) + "B";
    }
    else if(value >= 1024 && value < std::pow(1024, 2)) {
        value /= 1024;
        str = std::to_string(value) + "KB";
    }
    else if(value >= std::pow(1024, 2) && value < std::pow(1024, 3)) {
        double d_value = value / std::pow(1024, 2);
        str = double_to_string(d_value, 1);
        str += "MB";
    }
    else {
        double d_value = value / std::pow(1024, 3);
        str = double_to_string(d_value, 2);
        str += "GB";
    }
    return str;
}

std::string program_work_dir(const std::string& filter_word) {
    char cwd[PATH_MAX];
    if(!getcwd(cwd, sizeof(cwd))) {
        return std::string();
    }
    std::string dir(cwd);
    if(filter_word.empty()) {
        return dir;
    }
    StringSplitter sp(dir, '/');
    std::vector<std::string> dir_list;
    for(; sp; sp++) {
        dir_list.emplace_back(sp.field(), sp.length());
    }
    if(dir_list.back() == filter_word) {
        dir_list.pop_back();
    }
    std::string filter_dir("/");
    for(size_t i = 0; i < dir_list.size(); ++i) {
        filter_dir += dir_list.at(i);
        filter_dir += '/';
    }
    return filter_dir;
}

static pthread_once_t create_program_name_once = PTHREAD_ONCE_INIT;
static const char* s_program_name = "unknown";
static char s_cmdline[256];
static void CreateProgramName() {
    const ssize_t nr = ReadCommandLine(s_cmdline, sizeof(s_cmdline) - 1, false);
    if(nr > 0) {
        s_cmdline[nr] = '\0';
        s_program_name = s_cmdline;
    }
}

const char* program_name() {
    pthread_once(&create_program_name_once, CreateProgramName);
    return s_program_name;
}

const char* TabsHead() {
    return
        "<style type=\"text/css\">\n"
        "ol,ul { list-style:none; }\n"
        ".tabs-menu {\n"
        "    position: fixed;"
        "    top: 0px;"
        "    left: 0px;"
        "    height: 40px;\n"
        "    width: 100%;\n"
        "    clear: both;\n"
        "    padding: 0px;\n"
        "    margin: 0px;\n"
        "    background-color: #606060;\n"
        "    border:none;\n"
        "    overflow: hidden;\n"
        "    box-shadow: 0px 1px 2px #909090;\n"  
        "    z-index: 5;\n"
        "}\n"
        ".tabs-menu li {\n"
        "    float:left;\n"
        "    fill:none;\n"
        "    border:none;\n"
        "    padding:10px 30px 10px 30px;\n"
        "    text-align:center;\n"
        "    cursor:pointer;\n"
        "    color:#dddddd;\n"
        "    font-weight: bold;\n"
        "    font-family: \"Segoe UI\", Calibri, Arial;\n"
        "}\n"
        ".tabs-menu li.current {\n"
        "    color:#FFFFFF;\n"
        "    background-color: #303030;\n"
        "}\n"
        ".tabs-menu li.help {\n"
        "    float:right;\n"
        "}\n"
        ".tabs-menu li:hover {\n"
        "    background-color: #303030;\n"
        "}\n"
        "</style>\n"
        "<script type=\"text/javascript\">\n"
        "$(function() {\n"
        "  $(\".tabs-menu li\").click(function(event) {\n"
        "    window.location.href = $(this).attr('id');\n"
        "  });\n"
        "});\n"
        "</script>\n";
}

const char* logo() {
    return
        "    __\n"
        "   / /_  _________  _____\n"
        "  / __ \\/ ___/ __ \\/ ___/\n"
        " / /_/ / /  / /_/ / /__\n"
        "/_.___/_/  / .___/\\___/\n"
        "          /_/\n";
}



} // end namespace var