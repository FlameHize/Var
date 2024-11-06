#include "src/builtin/xvc_service.h"
#include "src/builtin/common.h"
#include "src/server.h"
#include "net/tcp/TcpServer.h"
#include "net/udp/UdpServer.h"

namespace var {

struct XvcInfo {
    std::string card_name;
    std::string card_ip;
    std::string user_ip;
    int         card_port;
    int         user_clk;
};

class XvcServer {
public:
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

    inline void add_xvc_info(const XvcInfo& info) {
        for(auto it : _info_list) {
            if(it.card_name == info.card_name) {
                return;
            }
        }
        _info_list.push_back(info);
    }

    inline void delete_xvc_info(const XvcInfo& info) {
        for(auto it = _info_list.begin(); it != _info_list.end(); ++it) {
            if(it->card_name == info.card_name) {
                _info_list.erase(it);
                return;
            }
        }
    }
    
    inline void start() {
        _tcp_server.start();
        _udp_server.start();
    } 

private:
    inline void on_vivado_onnection(const net::TcpConnectionPtr& conn) {
        if(conn->connected()) {
            _conn_list.push_back(conn);
        }
        else {
            for(auto iter = _conn_list.begin(); iter != _conn_list.end(); ++iter) {
                net::TcpConnectionPtr tmp = *iter;
                if(tmp->peerAddress().toIpPort() == conn->peerAddress().toIpPort()) {
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
        for(size_t i = 0; i < _info_list.size(); ++i) {
            XvcInfo& info = _info_list.at(i);
            if(info.card_ip == card_ip_str) {
                user_ip = info.user_ip;
                break;
            }
        }
        if(user_ip.empty()) {
            LOG_WARN << "Card ip: " << card_ip_str << "has not registered correspond user";
            return;
        }
        for(auto conn : _conn_list) {
            if(conn->peerAddress().toIp() != user_ip) {
                continue;
            }   
            conn->send(datagram.data(), datagram.size());
            break;
        }
    }

    net::TcpServer                      _tcp_server;
    net::UdpServer                      _udp_server;
    std::vector<XvcInfo>                _info_list;
    std::vector<net::TcpConnectionPtr>  _conn_list;
};

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
    response->set_body(os);
    return;
}

void XvcService::GetTabInfo(TabInfoList* info_list) const {
    TabInfo* info = info_list->add();
    info->path = "/xvc";
    info->tab_name = "FPGA虚拟线缆调试";
}

} // end namespace var