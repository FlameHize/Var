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

#ifndef VAR_PCIE_PCIELOOPTHREAD_H
#define VAR_PCIE_PCIELOOPTHREAD_H

#include "pcie/PCIeGlobal.h"
#include "Buffer.h"
#include "base/Mutex.h"
#include "base/Thread.h"
#include "base/Logging.h"

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <csignal>
#include <memory>
#include <functional>

namespace var {
namespace net {

#pragma pack(push, 1)

struct T_PCIEHEAD
{
    uint32_t uiHead;
    uint16_t usFrame;
    uint16_t usType;
    uint32_t uiLength;
    T_PCIEHEAD() {
        uiHead = 0xA1B2C3D4;
    }
};

#pragma pack(pop)

class PCIeLoopThread {
public:
    typedef std::function<void(const char* data, size_t len, uint type)> FileProcessCallback;

    explicit PCIeLoopThread(size_t h2c, size_t c2h, void* user_map, 
                            uint* sizedesign, const std::string& card_name);
    
    virtual ~PCIeLoopThread();

    void WriteToFPGA(const char* data, size_t len, uint type);

    void SetWriteDDRPtr(uint start, uint size);

    inline void SetFileProcessCallback(const FileProcessCallback& cb) {
        file_process_callback_ = cb;
    }

    inline void Start() {
        thread_.start();
    }

    inline void Quit() {
        run_flag_ = false;
    }

    inline int SetAffinity(size_t cpu_no) {
        return thread_.setAffinity(cpu_no);
    }

private:
    void RecvLoop();

    void GetData(size_t ddr_index, size_t file_num);

    void DoPendingWrite();

    void WriteInternal(const char* data, size_t len, uint type);

    void GetDDRData(size_t start, size_t size);

private:
    Thread thread_;
    std::string card_name_;
    bool run_flag_;

    size_t h2c_handler_;
    size_t c2h_handler_;
    void* user_map_;
    uint* ddr_size_design_;
    int file_index_[DDRSPLITSIZE];
    uint file_type_[DDRSPLITSIZE][SINGLEDDRSPLITSIZE];
    uint file_start_[DDRSPLITSIZE][SINGLEDDRSPLITSIZE];
    uint file_end_[DDRSPLITSIZE][SINGLEDDRSPLITSIZE];
    char* recv_buf_[DDRSPLITSIZE];

    size_t h2c_seek_;
    mutable MutexLock mutex_;
    std::vector<Buffer> pending_write_bufs GUARDED_BY(mutex_);

    int64_t read_bytes_count;
    int64_t write_bytes_count_;

    size_t write_start_ptr_;
    size_t write_end_ptr_;

    FileProcessCallback file_process_callback_;
};

} // end namespace net
} // end namespace var

#endif // VAR_PCIE_PCIELOOPTHREAD_H