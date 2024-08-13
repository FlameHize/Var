// Copyright 2024, Guangxu Zhu.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef VAR_NET_UDP_NETWORKDATAGRAM_H
#define VAR_NET_UDP_NETWORKDATAGRAM_H

#include "Buffer.h"
#include "InetAddress.h"
#include "Socket.h"

#include <memory>

namespace var {
namespace net {

class NetworkDatagram : public copyable {
public:
    NetworkDatagram(int sockfd, size_t buffer_size)
        : socket_(sockfd),
          buffer_(buffer_size) {
        
    }

    inline int sockfd() const {
        return socket_;
    } 

    // Sets the sender address associated with this datagram 
    // to be the address address and port number port. 
    // there's no need to call this function on a received datagram.
    inline void setSenderAddress(const InetAddress& addr) {
        peerAddr_ = addr;
    }

    // Returns the sender address associated with this datagram. 
    // For a datagram received from the network, it is the address 
    // of the peer node that sent the datagram. For an outgoing datagrams, 
    // it is the local address to be used when sending.
    inline const InetAddress& senderAddress() const {
        return peerAddr_;
    } 

    inline void setData(const char* data, const size_t len) {
        buffer_.append(data, len);
    }

    inline const char* data() const {
        return buffer_.peek();
    }

    inline const size_t size() const {
        return buffer_.readableBytes();
    }

private:
    int socket_;
    InetAddress peerAddr_;
    Buffer buffer_;
};

typedef std::shared_ptr<NetworkDatagram> NetworkDatagramPtr;

inline size_t WriteDatagram(int fd, 
                            const struct sockaddr* addr,
                            const char* data,
                            size_t len) {
    size_t n = ::sendto(fd, data, len, 0, addr, sizeof(*addr));
    return n;
}

inline size_t WriteDatagram(const NetworkDatagram& datagram) {
    return WriteDatagram(datagram.sockfd(), 
                         datagram.senderAddress().getSockAddr(),
                         datagram.data(),
                         datagram.size());
}

} // end namespace net
} // end namespace var

#endif