// Copyright 2024, Guangxu Zhu.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef VAR_NET_UDP_UDPSERVER_H
#define VAR_NET_UDP_UDPSERVER_H

#include "udp/NetworkDatagram.h"
#include "base/noncopyable.h"
#include "base/Thread.h"
#include "base/Timestamp.h"

#include <memory>
#include <functional>

namespace var {
namespace net {
namespace sockets {
    ///
    /// Creates a udp socket file descriptor,
    /// abort if any error.
    int createUdpServer(const struct sockaddr* addr);
} // end namespace sockets

class UdpServer : public noncopyable {
public:
    typedef std::function<void(const NetworkDatagram&, Timestamp)> DatagramCallback;

    explicit UdpServer(const InetAddress& listenAddr, const std::string& nameArg);
    virtual ~UdpServer();

    const std::string& ipPort() const { return ipPort_; }
    const std::string& name() const { return name_; }

    /// Starts the server if it's not listening.
    /// Not thread safe.
    void start();

    /// Set datagram callback.
    /// Not thread safe.
    void setDatagramCallback(const DatagramCallback& cb);

    // zhx excute in other thread.
    void SendData(const char* data,int size,char* addr,unsigned short port);

    size_t send(const char* data, size_t len, const InetAddress& addr);

private:
    void RecvLoop();
    

private:
    const string ipPort_;
    const string name_;
    const InetAddress listenAddr_;
    int listenfd_;
    int recv_buf_size_;
    Timestamp recv_time_;
    Thread thread_;
    DatagramCallback datagramCallback_;
};


} // end namespace net
} // end namespace var

#endif