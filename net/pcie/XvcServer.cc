#include "pcie/XvcServer.h"

using namespace var;
using namespace var::net;

XvcServer::XvcServer(const InetAddress& listenAddr, size_t fd)
    : server_(&loop_, listenAddr, "XvcServer-" + std::to_string(fd)) 
    , xdma_xvc_fd_(fd) {
    server_.setConnectionCallback(
        std::bind(&XvcServer::OnConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&XvcServer::OnMessage, this, _1, _2, _3));
}

XvcServer::~XvcServer() {
    loop_.quit();
}

void XvcServer::Start() {
    server_.start();
    loop_.loop();
}

void XvcServer::OnConnection(const TcpConnectionPtr& conn) {
    LOG_INFO << "XvcServer - " << conn->peerAddress().toIpPort() << " -> "
             << conn->localAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");
}

void XvcServer::OnMessage(const TcpConnectionPtr& conn,
                          Buffer* buf,
                          Timestamp time) {
    const char* data = buf->peek();
    if(buf->readableBytes() < 2) {
        // Data not enough, wait it.
        return;
    }
    // Resolved first 2 bytes, not consumed data.
    std::string cmd_str;
    cmd_str.append(data, 2);

    if(cmd_str == std::string("ge")) {
        // getinfo:
        if(buf->readableBytes() < 8) {
            // Data not enough, wait it.
            return;
        }
        char xvc_info[] = "xvcServer_v1.0:2048\n";
        conn->send(xvc_info, strlen(xvc_info));
        buf->retrieve(8);
        LOG_TRACE << "Recv command getinfo: and reply with: " << xvc_info; 
    }
    else if(cmd_str == std::string("se")) {
        // settck:F
        if(buf->readableBytes() < 11) {
            return;
        }
        int clock = 0;
        memcpy((char*)&clock, data + 7, sizeof(clock));
        conn->send((char*)&clock, sizeof(clock));
        buf->retrieve(11);
        LOG_TRACE << "Recv command settck:Int and reply with: " << clock << "ns";
    }
    else if(cmd_str == std::string("sh")) {
        // shift:<bitsLen><<tms><tdi>>
        if(buf->readableBytes() < 10) {
            // 10 = sizeof("shift:") + sizeof(bitslen)
            return;
        }
        int bits_len = 0;
        memcpy((char*)&bits_len, data + 6, sizeof(bits_len));
        int bytes_len = (bits_len + 7) / 8;
        int limit = 10 + 2 * bytes_len;
        if(buf->readableBytes() < limit) {
            return;
        }
        Buffer tms_buf;
        Buffer tmi_buf;
        Buffer tdo_buf;
        tms_buf.append(data + 10, bytes_len);
        tmi_buf.append(data + 10 + bytes_len, bytes_len);
        ShiftTmsTdi(tms_buf, tmi_buf, tdo_buf);
        if(tdo_buf.readableBytes() != 0) {
            conn->send(tdo_buf.peek(), tdo_buf.readableBytes());
        }
        buf->retrieve(limit);
    }
    else {
        // Close.
        buf->retrieveAll();
        conn->forceClose();
        LOG_ERROR << "Xvc error: " << errno;
    }
}

void XvcServer::ShiftTmsTdi(Buffer& tms_buf, Buffer& tdi_buf, Buffer& tdo_buf) {
    struct xvc_ioc xvc_fd_ioc;
    size_t len = tms_buf.readableBytes();
    char tdo[len];
    memset(tdo, 0, len);

    // 0x01:normal 0x02:bypass
    xvc_fd_ioc.opcode = 0x01;  
    xvc_fd_ioc.length = len;
    xvc_fd_ioc.tms_buf = tms_buf.peek();
    xvc_fd_ioc.tdi_buf = tdi_buf.peek();
    xvc_fd_ioc.tdo_buf = tdo;

    int ret = ioctl(xdma_xvc_fd_, XDMA_IOCXVC, &xvc_fd_ioc);
    if(ret < 0) {
        LOG_ERROR << "IOC error " << errno;
    }
    else {
        tdo_buf.append(tdo, len);
    }
}