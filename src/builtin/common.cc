#include "src/builtin/common.h"

namespace var {

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