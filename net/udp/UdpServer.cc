#include "udp/UdpServer.h"

#include "SocketsOps.h"
#include "base/Logging.h"

using namespace var;
using namespace var::net;

int sockets::createUdpServer(const struct sockaddr* addr) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(fd < 0) {
        LOG_SYSERR << "Failed to create udp server's socket";
        return -1;
    }
    // Set reuse addr and port
    int optval = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
            &optval, static_cast<socklen_t>(sizeof optval));
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
            &optval, static_cast<socklen_t>(sizeof optval));
    
    // bind
    sockets::bindOrDie(fd, addr);
    return fd;
}

UdpServer::UdpServer(const InetAddress& listenAddr,
                     const std::string& nameArg)
    : ipPort_(listenAddr.toIpPort()), 
      name_(nameArg),
      listenAddr_(listenAddr),
      recv_buf_size_(1472),
      thread_(std::bind(&UdpServer::RecvLoop, this), nameArg) { }

UdpServer::~UdpServer() {
    LOG_TRACE << "UdpServer::~UdpServer [" << name_ << "] destructing";
    thread_.join();
}

void UdpServer::start() {
    listenfd_ = sockets::createUdpServer(listenAddr_.getSockAddr());
    thread_.start();
}

void UdpServer::setDatagramCallback(const DatagramCallback& cb) {
    datagramCallback_ = cb;
}

void UdpServer::RecvLoop() {
    while(true) {
        // get network data from peer.
        char buffer[recv_buf_size_];
        sockaddr peer_addr;
        socklen_t addr_len = sizeof(peer_addr);

        // block here.
        int readn = ::recvfrom(listenfd_, buffer,
                               recv_buf_size_, 0, 
                               &peer_addr, &addr_len);
        if(readn >= 0) {
            recv_time_ = Timestamp::now();
            NetworkDatagram datagram(listenfd_, recv_buf_size_);
            const struct sockaddr_in* peer_addr_in = sockets::sockaddr_in_cast(&peer_addr);
            InetAddress peer(*peer_addr_in);
            datagram.setSenderAddress(peer);
            datagram.setData(buffer, readn);
            // call user's register logic func.
            datagramCallback_(datagram, recv_time_);
        }
        else {
            int eno = errno;
            LOG_ERROR << "errno = " << strerror(eno);
        }
    }
}

void UdpServer::SendData(const char* data,int size,char* addr,unsigned short port){
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(addr);
    
    sendto(listenfd_,data,size,0,(struct sockaddr*)&server_addr,sizeof(sockaddr_in));
}

size_t UdpServer::send(const char* data, size_t len, const InetAddress& addr) {
    NetworkDatagram datagram(listenfd_, len);
    datagram.setData(data, len);
    datagram.setSenderAddress(addr);
    return WriteDatagram(datagram);
}