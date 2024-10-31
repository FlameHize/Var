#include "src/builtin/common.h"
#include <sstream>
#include <iomanip>

namespace var {

// 将十六进制字符转换为整数  
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