#include "src/builtin/xvc_service.h"
#include "src/builtin/common.h"
#include "src/server.h"
#include "src/util/dir_reader_linux.h"
#include "src/util/file_reader_linux.h"
#include "net/base/FileUtil.h"
#include "src/var.h"
#include "src/util/json.hpp"
#include "net/tcp/TcpServer.h"
#include "net/udp/UdpServer.h"
#include <fstream>

namespace var {

enum XvcState {
    kDisconnect = 0,
    kConnectedToFPGA = 1,
    kConnectedToVivado = 2,
    kConnect = 3
};

#pragma pack(1)
struct SetTckOfXvc
{
    unsigned short head_flag;
    unsigned char  type;
    unsigned int   freq;
    unsigned short tail_flag;
    SetTckOfXvc() {
        head_flag = 0xAA55;
        type = 0x04;
        tail_flag = 0x55AA;
    }
};

struct GetTckOfXvc
{
    unsigned short head_flag;
    unsigned char  type;
    unsigned char  data;
    unsigned short tail_flag;
};

struct GetTdoOfXvc
{
    unsigned short head_flag;
    unsigned char  type;
};

struct XvcInfo {
    std::string    card_name;
    std::string    card_ip;
    std::string    user_ip;
    int            card_port;
    double         card_clk;
    net::Buffer    recv_buf;
    XvcState       state;
    std::shared_ptr<WindowEx<Adder<long long>>> speed;

    XvcInfo(const std::string& cardname,
            const std::string& cardip,
            const std::string& userip,
            int cardport,
            double cardclk) 
        : card_name(cardname)
        , card_ip(cardip)
        , user_ip(userip)
        , card_port(cardport)
        , card_clk(cardclk) {
        state = kDisconnect;
        speed = std::make_shared<WindowEx<Adder<long long>>>();
        speed->expose_as("xvc", card_name);
    }
};
#pragma pack()

class XvcServer {
public:
    friend class XvcService;
    inline XvcServer(net::EventLoop* loop, const net::InetAddress& addr) 
        : _tcp_server(loop, addr, "XvcTcpServer")
        , _udp_server(addr, "XvcUdpServer") {
        _tcp_server.setConnectionCallback(
            std::bind(&XvcServer::on_vivado_onnection, this, _1));
        _tcp_server.setMessageCallback(
            std::bind(&XvcServer::on_vivado_message, this, _1, _2, _3));
        _udp_server.setDatagramCallback(
            std::bind(&XvcServer::on_fpga_message, this, _1, _2));
    }

    inline void set_clock(const std::string& card_name) {
        for(auto& it : _info_list) {
            if(it.card_name == card_name) {
                int fre = 125 / it.card_clk;
                SetTckOfXvc opt;
                opt.freq = fre;
                net::InetAddress card_addr(it.card_ip, it.card_port);
                _udp_server.send((char*)&opt, sizeof(opt), card_addr);
                break;
            }
        }
    }

    inline bool add_xvc_info(const XvcInfo& info) {
        for(auto& it : _info_list) {
            if(it.card_name == info.card_name) {
                _error = std::string("card name repeat with already added card");
                return false;
            }
            if(it.card_ip == info.card_ip) {
                _error = std::string("card ip repeat with already added card");
                return false;
            }
            if(it.user_ip == info.user_ip) {
                _error = std::string("user ip repeat with already added card");
                return false;
            }
        }
        _info_list.push_back(info);
        return true;
    }

    inline bool delete_xvc_info(const std::string& card_name) {
        for(auto it = _info_list.begin(); it != _info_list.end(); ++it) {
            if(it->card_name == card_name) {
                _info_list.erase(it);
                return true;
            }
        }
        return false;
    }
    
    inline void start() {
        _tcp_server.start();
        _udp_server.start();
    } 

    inline std::string error() {
        return _error;
    }

private:
    inline void on_vivado_onnection(const net::TcpConnectionPtr& conn) {
        std::string vivado_ip = conn->peerAddress().toIp();
        for(auto& it : _info_list) {
            if(it.user_ip == vivado_ip) {
                if(conn->connected()) {
                    if(it.state == kDisconnect) {
                        it.state = kConnectedToVivado;
                    }
                    else if(it.state == kConnectedToFPGA) {
                        it.state = kConnect;
                    }
                    else {}
                }
                else {
                    if(it.state == kConnect) {
                        it.state = kConnectedToFPGA;
                    }
                    else if(it.state == kConnectedToVivado) {
                        it.state = kDisconnect;
                    }
                    else {}
                }
                break;
            }
        }
        if(conn->connected()) {
            _conn_list.push_back(conn);
        }
        else {
            for(auto iter = _conn_list.begin(); iter != _conn_list.end(); ++iter) {
                net::TcpConnectionPtr tmp = *iter;
                if(tmp->peerAddress().toIp() == vivado_ip) {
                    _conn_list.erase(iter);
                    break;
                }
            }
        }
        LOG_INFO << "Vivado - " << conn->peerAddress().toIpPort() << " -> "
                 << conn->localAddress().toIpPort() << " is "
                 << (conn->connected() ? "UP" : "DOWN");
    }

    inline void on_vivado_message(const net::TcpConnectionPtr& conn,
                                  net::Buffer* buf,
                                  Timestamp time) {
        const char* data = buf->peek();
        if(buf->readableBytes() < 2) {
            return;
        }
        net::Buffer cmd_bytes;
        cmd_bytes.append(data, 2);
        std::string cmd_str = cmd_bytes.retrieveAllAsString();
        if(cmd_str == std::string("ge")) {
            // getinfo:
            if(buf->readableBytes() < 8) {
                return;
            }
            char xvc_info[] = "xvcServer_v1.0:2048\n";
            net::Buffer reply;
            reply.append(xvc_info, strlen(xvc_info));
            conn->send(reply.peek(), reply.readableBytes());
            buf->retrieve(8);
            LOG_INFO << "XvcServer receive command: 'getinfo' "
                     << "and reply with " << xvc_info;
        }
        else if(cmd_str == std::string("se")) {
            // settck:(int)F
            if(buf->readableBytes() < 11) {
                return;
            }
            int clk = 0;
            memcpy((char*)&clk, data + 7, sizeof(clk));
            conn->send((char*)&clk, sizeof(clk));
            buf->retrieve(8);
            LOG_INFO << "XvcServer receive command: 'settck' "
                     << "and reply with " << clk << "ns";
        }
        else if(cmd_str == std::string("sh")) {
            // shift:(int)N
            if(buf->readableBytes() < 10) {
                return;
            }
            int bits_len = 0;
            memcpy((char*)&bits_len, data + 6, sizeof(bits_len));
            int bytes_len = (bits_len + 7) / 8;
            int limit = 10 + bytes_len * 2;
            if(buf->readableBytes() < static_cast<size_t>(limit)) {
                return;
            }
            net::Buffer reply;
            unsigned short head_flag = 0xCF1A;
            unsigned char  tms_type = 0x00;
            unsigned char  tdi_type = 0x01;
            unsigned short tail_flag = 0x1ACF;
            buf->retrieve(10);

            // TMS.
            reply.append((char*)&head_flag, sizeof(head_flag));
            reply.append((char*)&tms_type, sizeof(tms_type));
            reply.append((char*)&bits_len, sizeof(bits_len));
            reply.append(buf->peek(), bytes_len);
            reply.append((char*)&tail_flag, sizeof(tail_flag));
            buf->retrieve(bytes_len);

            // TDI.
            reply.append((char*)&head_flag, sizeof(head_flag));
            reply.append((char*)&tdi_type, sizeof(tdi_type));
            reply.append((char*)&bits_len, sizeof(bits_len));
            reply.append(buf->peek(), bytes_len);
            reply.append((char*)&tail_flag, sizeof(tail_flag));
            buf->retrieve(bytes_len);

            bool find_card = false;
            std::string user_ip_str = conn->peerAddress().toIp();
            for(size_t i = 0; i < _info_list.size(); ++i) {
                XvcInfo& info = _info_list.at(i);
                if(info.user_ip != user_ip_str) {
                    continue;
                }
                find_card = true;
                net::InetAddress card_ip_port(info.card_ip, info.card_port);
                _udp_server.send(reply.peek(), 
                                 reply.readableBytes(), 
                                 card_ip_port);
                break;
            }
            if(!find_card) {
                LOG_WARN << "User ip: " << user_ip_str << " has not registered correspond card";
            }
        }
        else {
            LOG_WARN << "XvcTcpServer recv data absolute wrong";
            buf->retrieveAll();
            return;
        }
    }

    inline void on_fpga_message(const net::NetworkDatagram& datagram, 
                                Timestamp time) {
        const net::InetAddress& card_ip = datagram.senderAddress();
        std::string card_ip_str = card_ip.toIp();
        std::string user_ip;
        std::string card_name;
        net::Buffer* buf = nullptr;
        XvcState* state = nullptr;
        std::shared_ptr<WindowEx<Adder<long long>>> speed;
        for(size_t i = 0; i < _info_list.size(); ++i) {
            XvcInfo& info = _info_list.at(i);
            if(info.card_ip == card_ip_str) {
                user_ip = info.user_ip;
                card_name = info.card_name;
                buf = &info.recv_buf;
                state = &info.state;
                speed = info.speed;
                break;
            }
        }
        if(user_ip.empty()) {
            LOG_WARN << "Card ip: " << card_ip_str << "has not registered correspond user";
            return;
        }
        if(buf->readableBytes() >= sizeof(GetTckOfXvc)) {
            GetTckOfXvc opt;
            memcpy((char*)&opt, buf->peek(), sizeof(opt));
            if(opt.head_flag == 0xAA55 && opt.tail_flag == 0x55AA) {
                if(*state == kDisconnect) {
                    *state = kConnectedToFPGA;
                }
                else if(*state == kConnectedToVivado) {
                    *state = kConnect;
                }
                else {}
                LOG_INFO << card_name << "'s clk setting success";
                buf->retrieve(sizeof(opt));
            }
        }
        while(buf->readableBytes() > sizeof(GetTdoOfXvc) + sizeof(ushort)) {
            ushort size = 0;
            memcpy((char*)&size, buf->peek() + 3, 2);
            // 7 = sizeof(GetTdoOfXvc)(0X1ACF + 0X02) + sizeof(ushort) + sizeof(tail)(0xCF1A).
            if(buf->readableBytes() < static_cast<size_t>(size + 7)) {
                return;
            }
            net::Buffer temp;
            buf->retrieve(5);
            temp.append(buf->peek(), size);
            buf->retrieve(size);
            buf->retrieve(2);

            for(auto conn : _conn_list) {
                if(conn->peerAddress().toIp() != user_ip) {
                    continue;
                }   
                conn->send(temp.peek(), temp.readableBytes());
                *speed << temp.readableBytes();
                break;
            }
        }
    }

    net::TcpServer                      _tcp_server;
    net::UdpServer                      _udp_server;
    std::vector<XvcInfo>                _info_list;
    std::vector<net::TcpConnectionPtr>  _conn_list;
    std::string                         _error;
};

static XvcServer* g_xvc_server = nullptr;
const std::string XvcFileSaveDir = "data/xvc/";

void xvc_server_init() {
    net::EventLoop loop;
    net::InetAddress addr(2542);
    if(!g_xvc_server) {
        g_xvc_server = new XvcServer(&loop, addr);
    }
    // Recover already existed xvc config.
    std::string path = XvcFileSaveDir + "meta";
    std::ifstream meta(path, std::ios::in);
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
            std::string card_name(sp.field(), sp.length());
            sp++;
            std::string card_ip(sp.field(), sp.length());
            sp++;
            std::string card_port(sp.field(), sp.length());
            sp++;
            std::string user_ip(sp.field(), sp.length());
            sp++;
            std::string card_clock(sp.field(), sp.length());
            
            XvcInfo info(card_name, card_ip, user_ip, 
                            std::stoi(card_port), std::stod(card_clock));
            g_xvc_server->add_xvc_info(info);
            g_xvc_server->set_clock(card_name);
        }
    }
    g_xvc_server->start();
    loop.loop();
}

XvcService::XvcService() 
    : _thread(xvc_server_init, "xvc_service") {
    AddMethod("add_card", std::bind(&XvcService::add_card,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("add_card_internal", std::bind(&XvcService::add_card_internal,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("delete_card", std::bind(&XvcService::delete_card,
                this, std::placeholders::_1, std::placeholders::_2));
    AddMethod("update", std::bind(&XvcService::update,
                this, std::placeholders::_1, std::placeholders::_2));
    _thread.start();
}

void XvcService::add_card(net::HttpRequest* request,
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
        os << "<form id=\"card-form\" enctype=\"multipart/form-data\">\n"
        "       <label for=\"card-name\">板卡名称:</label>\n"
        "       <input type=\"text\" id=\"card-name\" name=\"card-name\" placeholder=\"请输入英文字符\">\n"
        "       <label for=\"card-ip\">板卡IP:</label>\n"
        "       <input type=\"text\" id=\"card-ip\" name=\"card-ip\" placeholder=\"FPGA调试板烧写IP\">\n"
        "       <label for=\"card-port\">板卡Port:</label>\n"
        "       <input type=\"text\" id=\"card-port\" name=\"card-port\" placeholder=\"FPGA调试板烧写Port\">\n"
        "       <label for=\"user-ip\">用户IP:</label>\n"
        "       <input type=\"text\" id=\"user-ip\" name=\"user-ip\" placeholder=\"运行Vivado调试的主机IP\">\n"
        "       <label for=\"selected-clock\">时钟速率(MHz):</label>\n"
        "       <select name=\"card-clock\" id=\"selected-clock\" style=\"min-width:60px;\">\n"
        "       <option value=\"30\">30</option>\n"
        "       <option value=\"20\">20</option>\n"
        "       <option value=\"12.5\">12.5</option>\n"
        "       <option value=\"5\">5</option>\n"
        "       <option value=\"3.125\">3.125</option>\n"
        "       <option value=\"2\">2</option>\n"
        "       <option value=\"1\">1</option>\n"
        "       <option value=\"0.5\">0.5</option>\n"
        "       </select>\n"
        "       <button type=\"button\" onclick=\"submitForm()\">确定</button>\n"
        "</form>\n"
        "<script>\n"
        "function submitForm() {\n"
        "    var cardName = document.getElementById('card-name').value;\n"
        "    var cardIp = document.getElementById('card-ip').value;\n"
        "    var cardPort = document.getElementById('card-port').value;\n"
        "    var userIp = document.getElementById('user-ip').value;\n"
        "    var cardClock = $('#selected-clock option:selected').text();\n"
        "    if(cardName.trim() == '') {\n"
        "        alert('板卡名称为空，请输入');\n"
        "        return;\n"
        "    }\n"
        "    if(cardIp.trim() == '') {\n"
        "        alert('板卡IP为空，请输入');\n"
        "        return;\n"
        "    }\n"
        "    if(cardPort.trim() == '') {\n"
        "        alert('板卡Port为空，请输入');\n"
        "        return;\n"
        "    }\n"
        "    if(userIp.trim() == '') {\n"
        "        alert('用户IP为空，请输入');\n"
        "        return;\n"
        "    }\n"
        "    $.ajax({\n"
        "    url: '/xvc/add_card_internal',\n"
        "    type: \"POST\",\n"
        "    data: JSON.stringify({\n"
        "        cardName: cardName,\n"
        "        cardIp: cardIp,\n"
        "        cardPort: cardPort,\n"
        "        userIp: userIp,\n"
        "        cardClock: cardClock\n"
        "    }),\n"
        "    processData: false,\n"
        "    contentType: false,\n"
        "    success: function(response) {\n"
        "        alert('导入调试板卡信息成功');\n"
        "        window.location.href = '/xvc';\n"
        "    },\n"
        "    error: function(error) {\n"
        "        var errStr = \"导入调试板卡信息失败: \" + error.responseText;"
        "        alert(errStr);\n"
        "    }\n"
        "    });\n"
        "}\n"
        "document.getElementById('card-name').addEventListener('input', function(event) {\n"
        "   var value = event.target.value;\n"
        "   var regex = /^[A-Za-z0-9]*$/;\n"
        "   if(!regex.test(value)) {\n"
        "       event.preventDefault();\n"
        "       event.target.value = value.slice(0,-1);\n"
        "   }\n"
        "   this.value = this.value.replace(/[\u4e00-\u9fa5]/g, '');\n"
        "});\n"
        "document.getElementById('card-port').addEventListener('input', function(event) {\n"
        "   var value = event.target.value;\n"
        "   var regex = /^[0-9]*$/;\n"
        "   if(!regex.test(value)) {\n"
        "       event.preventDefault();\n"
        "       event.target.value = value.slice(0,-1);\n"
        "   }\n"
        "   this.value = this.value.replace(/[\u4e00-\u9fa5]/g, '');\n"
        "});\n"
        "document.getElementById('card-port').addEventListener('blur', function(event) {\n"
        "   const port = parseInt(event.target.value);\n"
        "   if(port < 0 || port > 65536) {\n"
        "       alert('无效端口号，请检查(合法端口号范围:[0-65536])');\n"
        "       event.target.value = '';\n"
        "   }\n"
        "});\n"
        "function vaildIpAddress(event) {\n"
        "   const ip = event.target.value;\n"
        "   const ipv4Pattern = /^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.)"
        "{3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;\n"
        "   const isVaild = ipv4Pattern.test(ip);\n"
        "   if(!isVaild && ip != '') {\n"\
        "       alert('无效IP地址，请检查');\n"
        "       event.target.value = '';\n"
        "   }\n"
        "}\n"
        "document.getElementById('card-ip').addEventListener('blur', vaildIpAddress);\n"
        "document.getElementById('user-ip').addEventListener('blur', vaildIpAddress);\n"
        "</script>\n";
    }

    if(use_html) {
        os << "</body></html>";
    }
    response->set_body(os);
}

void XvcService::add_card_internal(net::HttpRequest* request,
                                   net::HttpResponse* response) {
    std::string body_str = request->body().retrieveAllAsString();
    nlohmann::json body_json = nlohmann::json::parse(body_str);
    std::string card_name = body_json["cardName"];
    std::string card_ip = body_json["cardIp"];
    std::string card_port = body_json["cardPort"];
    std::string user_ip = body_json["userIp"];
    std::string card_clock = body_json["cardClock"];

    net::BufferStream os;
    if(card_name.empty() || card_ip.empty() || card_port.empty() ||
        user_ip.empty() || card_clock.empty()) {
        os << "Card info is incomplete";
        response->header().set_status_code(net::HTTP_STATUS_BAD_REQUEST);
        response->set_body(os);
        return;
    }
    XvcInfo info(card_name, card_ip, user_ip, 
                 std::stoi(card_port), std::stod(card_clock));
    if(!g_xvc_server) {
        os << "Xvc server has not running";
        response->header().set_status_code(net::HTTP_STATUS_BAD_REQUEST);
        response->set_body(os);
        return;
    }
    if(!g_xvc_server->add_xvc_info(info)) {
        os << g_xvc_server->error();
        response->header().set_status_code(net::HTTP_STATUS_BAD_REQUEST);
        response->set_body(os);
        return;
    }
    g_xvc_server->set_clock(card_name);

    std::string path = XvcFileSaveDir;
    if(!DirReaderLinux::CreateDirectoryIfNotExists(path.c_str())) {
        os << "Xvc file " << path << " dir not existed";
        response->header().set_status_code(net::HTTP_STATUS_BAD_REQUEST);
        response->set_body(os);
        return;
    }
    path += "meta";
    std::ifstream meta(path, std::ios::in);
    net::BufferStream meta_os;
    meta_os << card_name << ' '
                << card_ip << ' '
                << card_port << ' '
                << user_ip << ' '
                << card_clock;
    net::Buffer meta_info;
    meta_os.moveTo(meta_info);
    std::string meta_line = meta_info.retrieveAllAsString();
    if(!meta) {
        FileUtil::AppendFile meta_file(path);
    }
    std::ofstream meta_out(path, std::ios_base::app);
    meta_out << meta_line << "\n";
    meta_out.close();
    LOG_INFO << "Success add a new xvc device: " << meta_line;
}

void XvcService::delete_card(net::HttpRequest* request,
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
        const std::string* name = request->header().url().GetQuery("cardname");
        if(!name) {
            os << "<form id=\"form_card\" method=\"get\">\n" 
            << "<label for=\"selected-card\">选择板卡</label>\n"
            << "<select name=\"cardname\" id=\"selected-card\" style=\"min-width:60px;\">\n";
            std::vector<XvcInfo>& info_list = g_xvc_server->_info_list;
            for(size_t i = 0; i < info_list.size(); ++i) {
                XvcInfo& info = info_list.at(i);
                std::string card_name = info.card_name;
                os << "<option value=\"" << card_name << "\">"
                << card_name  << "</option>\n";
            }
            os << "</select>\n"
            << "<input type=\"submit\" value=\"删除\"></form>";
        }
        else {
            if(!g_xvc_server) {
                os << "<script>alert('Xvc server has not running');</script>";
                response->set_body(os);
                return;
            }
            if(!g_xvc_server->delete_xvc_info(*name)) {
                os << "<script>alert('" << g_xvc_server->error() << "');</script>";
                response->set_body(os);
                return;
            }
            std::string path = XvcFileSaveDir;
            if(!DirReaderLinux::DirectoryExists(path.c_str())) {
                LOG_ERROR << "Wait deleted card file dir" << path << "not existed";
                os << "<script>alert(Delete " << *name << " failed);</script>";
                response->set_body(os);
                return;
            }
            path += "meta";
            std::ifstream meta(path, std::ios::in);
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
                    if(meta_name == *name) {
                        meta_lines.erase(iter);
                        break;
                    }
                }
                std::ofstream meta_out(path);
                for(size_t i = 0; i < meta_lines.size(); ++i) {
                    meta_out << meta_lines.at(i) << "\n";
                }
                meta_out.close();
            }
            else {
                os << "<script>alert('Delete xvc meta info"
                "file not existed');</script>";
                response->set_body(os);
                return;
            }
            meta.close();
            os << "<script>alert('已删除板卡" << *name << "');\n"
            "        window.location.href=\"/xvc\";\n"
            "</script>\n";
            LOG_INFO << "Success deleta a xvc device: " << *name;
        }
    }
    if(use_html) {
        os << "</body></html>";
    }
    response->set_body(os);
}

void XvcService::update(net::HttpRequest* request,
                        net::HttpResponse* response) {
    net::BufferStream os;
    std::vector<XvcInfo>& info_list = g_xvc_server->_info_list;
    for(size_t i = 0; i < info_list.size(); ++i) {
        XvcInfo& info = info_list.at(i);
        os << info.state;
        if(i != info_list.size() - 1) {
            os << '_';
        }
    }
    response->set_body(os);
    return;
}


void XvcService::default_method(net::HttpRequest* request,
                                net::HttpResponse* response) {
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
        server->PrintTabsBody(os, "FPGA虚拟线缆调试");
    }

    // Card operation layer.
    if(use_html) {
        os << "<form id=\"form_card\" method=\"get\">\n"; 
        os << "<input type='button' "
           << "onclick='location.href=\"/xvc/add_card\";' "
           << "value='添加板卡'>\n";
        os << "<input type='button' "
           << "onclick='location.href=\"/xvc/delete_card\";' "
           << "value='删除板卡'>\n";
        os << "</form>\n";
    }

    // Xvc config list.
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
        "   <th>板卡名称</th>\n"
        "   <th>板卡IP</th>\n"
        "   <th>板卡Port</th>\n"
        "   <th>用户IP</th>\n"
        "   <th>时钟速率(MHz)</th>\n"
        "   <th>传输速度</th>\n"
        "   <th>当前状态</th>\n"
        "</tr>\n";
        std::vector<XvcInfo>& info_list = g_xvc_server->_info_list;
        for(size_t i = 0; i < info_list.size(); ++i) {
            XvcInfo& info = info_list.at(i);
            os << "<tr>\n";
            os << "<td>" << info.card_name << "</td>\n";
            os << "<td>" << info.card_ip << "</td>\n";
            os << "<td>" << info.card_port << "</td>\n";
            os << "<td>" << info.user_ip << "</td>\n";
            os << "<td>" << info.card_clk << "</td>\n";
            os << "<td>" << info.speed->get_value() << "</td>\n";
            os << "<td class=\"state\"></td>\n";
            os << "</tr>\n";
        }
        os << "</table>\n";
    }

    // State check interval.
    if(use_html) {
        os << "<script>\n"
            "var timerId;\n"
            "function stateToString(val) {\n"
            "   var result;\n"
            "   if(val == 0) {\n"
            "       result = \"未运行\";\n"
            "   }\n"
            "   else if(val == 1) {\n"
            "       result = \"时钟速率设置成功，等待Vivado连接\";\n"
            "   }\n"
            "   else if(val == 2) {\n"
            "       result = \"Vivado已连接，等待时钟速率回复\";\n"
            "   }\n"
            "   else if(val == 3) {\n"
            "       result = \"运行中\";\n"
            "   }\n"
            "   else {\n"
            "       result = \"未知\";\n"
            "   }\n"
            "   return result;\n"
            "}\n"
            "function fetchData() {\n"
                "$.ajax({\n"
                "url: \"/xvc/update\",\n"
                "type: \"GET\",\n"
                "dataType: \"html\",\n"
                "success: function(data) {\n"
                "   if(data.trim() === '') {\n"
                "       clearInterval(timerId);\n"
                "       return;\n"
                "   }\n"
                "   let states = data.split('_');\n"
                "   let elements = document.getElementsByClassName('state');\n"
                "   for(let i = 0; i < elements.length; ++i) {\n"
                "       elements[i].innerHTML = stateToString(states[i]);\n"
                "   }\n"
                "}\n"
            "});\n"
        "}\n"
        "timerId = setInterval(fetchData, 1000);\n"
        "</script>\n";
    }

    if(use_html) {
        os << "</body></html>\n";
    }
    response->set_body(os);
    return;
}

void XvcService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/xvc";
    info->tab_name = "FPGA虚拟线缆调试";
}

} // end namespace var