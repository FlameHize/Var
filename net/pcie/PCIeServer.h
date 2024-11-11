// Copyright (c) 2024.06.24 zgx
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

#ifndef VAR_PCIE_PCIESERVER_H
#define VAR_PCIE_PCIESERVER_H

#include "pcie/PCIeLoopThread.h"
#include "pcie/XvcServer.h"

namespace var {
namespace net {

class PCIeServer {
public:
    explicit PCIeServer(size_t channel_size = 1);
    virtual ~PCIeServer();

    bool OpenXDMADriver(const std::string& driver_path);

    bool SetCardId(const std::string& card_name, 
                   int card_id);

    bool SetCardDDRInfo(const std::string& card_name, 
                        int ddr_index, 
                        int ddr_start, 
                        int ddr_size);

    inline void EnableXVCDebug() { enable_xvc_ = true; }

    bool SetCardDataProcessFunc(const std::string& card_name, 
                                const PCIeLoopThread::FileProcessCallback& cb);

    void WriteToFPGA(const std::string& card_name, 
                     uint type, 
                     const char* data, 
                     size_t len);

    void Start();

private:
    void OnDefaultPCIeFileData(const char* data, size_t len, uint type);

private:
    size_t channel_size_;
    size_t user_handler_[XDMACHANNELNUM];
    size_t c2h_handler_[XDMACHANNELNUM];
    size_t h2c_handler_[XDMACHANNELNUM];
    size_t h2c_seek_[XDMACHANNELNUM];
    size_t xvc_handler_[XDMACHANNELNUM];
    void* user_map_[XDMACHANNELNUM];
    uint ddr_size_design_[XDMACHANNELNUM][DDRSPLITSIZE + 1][2];

    PCIeLoopThread* loop_thread_[XDMACHANNELNUM];

    std::map<std::string, int> name_to_card_id_;
    std::map<std::string, int> name_to_channel_id_;
    std::map<std::string, PCIeLoopThread::FileProcessCallback> name_to_process_func_;

    bool enable_xvc_;
    std::vector<XvcServer*> xvc_server_list_;
};

} // end namespace net
} // end namespace var

#endif // VAR_PCIE_PCIESERVER_H