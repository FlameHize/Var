// Copyright (c) 2024.08.13 zgx
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the disruptor-- nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL FRANCOIS SAINT-JACQUES BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef VAR_PCIE_XVCSERVER_H
#define VAR_PCIE_XVCSERVER_H

#include "pcie/PCIeGlobal.h"
#include "tcp/TcpServer.h"
#include "base/Logging.h"
#include "base/Thread.h"
#include "EventLoop.h"
#include <sys/ioctl.h>
#include <fcntl.h>

namespace var {
namespace net {
/* the bar offset can be changed at compile time*/
#define XVC_BAR_OFFSET_DFLT	0x40000	/* DSA 4.0 */

#define XVC_MAGIC 0x58564344  // "XVCD"

struct xvc_ioc {
	unsigned int opcode;
	unsigned int length;
	const char*  tms_buf;
	const char*  tdi_buf;
	void*        tdo_buf;
};

#define XDMA_IOCXVC	_IOWR(XVC_MAGIC, 1, struct xvc_ioc)

class XvcServer {
public:
    explicit XvcServer(const InetAddress& listenAddr, size_t fd);
    virtual ~XvcServer();
    void Start();

private:
    void OnConnection(const TcpConnectionPtr& conn);

    void OnMessage(const TcpConnectionPtr& conn,
                   Buffer* buf,
                   Timestamp time);

    /* Called when the shift: command is received to perform <count>
     * TCK. <tms_buf> and <tdi_buf> contain TMS and TDI values for each clock.  
     * <tdo_buf> should be populated for each clock from TDO.
    */
    void ShiftTmsTdi(Buffer& tms_buf, Buffer& tdi_buf, Buffer& tdo_buf);

private:
    size_t xdma_xvc_fd_;
    TcpServer server_;
    EventLoop loop_;
};

class XvcServerAsync {
public:
    inline XvcServerAsync(const InetAddress& listenAddr, size_t fd)
        : addr_(listenAddr)
        , fd_(fd)
        , thread_(std::bind(&XvcServerAsync::Run, this)) {
    }

    inline virtual ~XvcServerAsync() {
        thread_.join();
    }

    inline void Start() {
        thread_.start();
    }

private:
    inline void Run() {
        server_ = std::make_unique<XvcServer>(addr_, fd_);
        server_->Start();
    }

private:
    InetAddress addr_;
    size_t fd_;
    std::unique_ptr<XvcServer> server_;
    Thread thread_;
};

} // end namespace net
} // end namespace var


#endif