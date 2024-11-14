#include "pcie/PCIeLoopThread.h"

using namespace var;
using namespace var::net;

PCIeLoopThread::PCIeLoopThread(size_t h2c, size_t c2h, void* user_map, 
                uint* sizedesign, const std::string& card_name) 
    : h2c_handler_(h2c)
    , c2h_handler_(c2h)
    , user_map_(user_map)
    , ddr_size_design_(sizedesign)
    , card_name_(card_name) 
    , run_flag_(true)
    , h2c_seek_(0)
    , read_bytes_count(0)
    , write_bytes_count_(0)
    , write_start_ptr_(0)
    , write_end_ptr_(0)
    , thread_(std::bind(&PCIeLoopThread::RecvLoop, this), card_name) {
    for(size_t i = 0; i < DDRSPLITSIZE; ++i) {
        file_index_[i] = 0;
    }
    // Alloc DDR size.
    size_t alloc_memory_flag = 1;
    for(size_t i = 0; i < DDRSPLITSIZE; ++i) {
        alloc_memory_flag = posix_memalign((void **)&recv_buf_[i], ALIGN_SIZE, PACKAGE_SIZE + ALIGN_SIZE);
        if(alloc_memory_flag == ENOMEM) {
            LOG_SYSERR << "PCIe buf memory allocation failed with error: " << alloc_memory_flag;
            run_flag_ = false;
            break;
        }
    }
}

PCIeLoopThread::~PCIeLoopThread() {
    thread_.join();
}

void PCIeLoopThread::RecvLoop() {
    // Register signal.
    signal(SIGINT, [](int signal) -> void {
        exit(signal);
    });

    while(run_flag_) {
        for(size_t i = 0; i < DDRSPLITSIZE; ++i) {
            // 从FPGA Bar空间中读取各DDR分区当前对应的文件索引 0/1/2/3
            int index = *(uint32_t*)((uint8_t*)user_map_ + 4 * (1 + 10 * i));
            index &= 0x3;

            // 读取文件类型
            int type  = *(uint32_t*)((uint8_t*)user_map_ + 4 * (2 + 10 * i));

            // 缓存各DDR分区当前待读取的文件类型(用于寻回) 
            // type为32bit 8bit为一个寻回的类型
            for(size_t j = 0; j < SINGLEDDRSPLITSIZE; ++j) {
                file_type_[i][j] = ((type >> (8 * j)) & 0xff);
            }

            // 文件寻回机制(最大寻回数量为4)
            if(file_index_[i] != index) {
                size_t wait_process_num = 0;
                if(index > file_index_[i]) {
                    wait_process_num = index - file_index_[i];
                }
                else {
                    // 开启寻回 寻回数量最大为4
                    wait_process_num = index + 4 - file_index_[i];
                }
                // 6个DDR分区依次处理待读取的数据
                for(size_t j = 0; j < wait_process_num; ++j) {
                    GetData(i, j);
                }
            }

            // 处理完待读取数据 缓存当前DDR分区的待读取文件索引
            file_index_[i] = index;

            // Write data to PCIe.
            DoPendingWrite();
        }
    }
    LOG_INFO << card_name_ << "thread quit recv loop";
    free(recv_buf_);
}

void PCIeLoopThread::GetData(size_t ddr_index, size_t file_num) {
    // Calculate next file index. example:
    // has resolved file_index 3 -> file_index_[ddr_index] == 3
    // wait process file index: 0 -> file_num == 0
    // calculate file_index is 0, which is next real waiting process file index.
    uint file_index = (file_index_[ddr_index] + file_num + 1) % 4;

    // Get file start and end pos.
    file_start_[ddr_index][file_index] = *(uint32_t*)((uint8_t*)user_map_ + 4 * (3 + file_index * 2 + 10 * ddr_index));
    file_end_[ddr_index][file_index] = *(uint32_t*)((uint8_t*)user_map_ + 4 * ( 4 + file_index * 2 + 10 * ddr_index));

    // Used to locate file in memory.
    int file_memory_start = file_start_[ddr_index][file_index];
    int file_memory_end = file_end_[ddr_index][file_index];

    // Locate the data to be read from the c2h channel.
    ::lseek(c2h_handler_, file_memory_start, SEEK_SET);

    // Calculate file size to be read.
    int file_size = file_memory_end - file_memory_start;

    // Used to save acutal readed file size.
    int read_size = 0;

    // Cycle reading.
    if(file_size >= 0) {
        read_size = file_size;
        if(read_size > DATA_SIZE) {
            read_size = DATA_SIZE;
        }
        ::read(c2h_handler_, recv_buf_[ddr_index], read_size);
    }
    else {
        ///@todo fix: un-safe. 
        // ddr_size_design_[ddr_index][0] ddr start
        // ddr_size_design_[ddr_index][1] ddr size
        // it's sequence, so we need to * 2.
        int ddr_memory_start = *(ddr_size_design_ + ddr_index * 2 + 0);
        int ddr_memory_size = *(ddr_size_design_ + ddr_index * 2 + 1);

        // Calculate tail and head spilite file size.
        int tail_file_size = ddr_memory_start + ddr_memory_size - file_memory_start;
        int head_file_size = file_memory_end - ddr_memory_start;

        // Read tail splite file.
        ::read(c2h_handler_, recv_buf_[ddr_index], tail_file_size);

        // Reseek channel to ddr start for reading head file size.
        ::lseek(c2h_handler_, ddr_memory_start, SEEK_SET);

        // Read head splite file.
        ::read(c2h_handler_, recv_buf_[ddr_index] + tail_file_size, head_file_size);

        // Calculate total read file size.
        read_size = tail_file_size + head_file_size;
    }

    // log.
    read_bytes_count += read_size;

    // limit to package size.
    if(read_size > PACKAGE_SIZE){
        read_size = PACKAGE_SIZE;
    }

    // sync data to user logic func.
    file_process_callback_(recv_buf_[ddr_index], read_size, file_type_[ddr_index][file_index]);    
}

void PCIeLoopThread::WriteToFPGA(const char* data, size_t len, uint type) {
    if(thread_.tid() == CurrentThread::tid()) {
        /// direct write.
        WriteInternal(data, len ,type);
    }
    else {
        // queueinloop().
        Buffer buffer;
        buffer.append((char*)&type, sizeof(type));
        buffer.append(data, len);
        {
            MutexLockGuard lock(mutex_);
            pending_write_bufs.push_back(buffer);
        }
    }
}

void PCIeLoopThread::DoPendingWrite() {
    std::vector<Buffer> bufs;
    {
        MutexLockGuard lock(mutex_);
        bufs.swap(pending_write_bufs);
    }
    for(Buffer& buf : bufs) {
        uint type = 0;
        memcpy((char*)&type, buf.peek(), sizeof(type));
        buf.retrieve(sizeof(type));
        WriteInternal(buf.peek(), buf.readableBytes(), type);
    }
}

void PCIeLoopThread::WriteInternal(const char* data, size_t len, uint type) {
    int already_write_bytes = 0;    
    int sendLen = len + sizeof(T_PCIEHEAD);
    int temp_count = sendLen % ONCEBYTES;
    temp_count = temp_count == 0 ? sendLen / ONCEBYTES : sendLen / ONCEBYTES + 1;
    int padding_count = temp_count * ONCEBYTES - sizeof(T_PCIEHEAD) - len;
    T_PCIEHEAD tPCIEHEAD;
    tPCIEHEAD.usType = type;
    tPCIEHEAD.uiLength = len;
    net::Buffer buf;
    buf.append((char*)&tPCIEHEAD, sizeof(tPCIEHEAD));
    buf.append(data, len);
    char c = 0;
    for(int i = 0; i < padding_count; ++i) {
        buf.append((char*)&c, sizeof(c));
    }
    int expect_write_bytes = buf.readableBytes();
    int actual_write_bytes = 0;
    while(temp_count) {
        if(h2c_seek_ >= write_end_ptr_) { 
            h2c_seek_ = write_start_ptr_;
        }
        ::lseek(h2c_handler_, h2c_seek_, SEEK_SET);
        int n = ::write(h2c_handler_, buf.peek() + already_write_bytes, ONCEBYTES * sizeof(char));
        actual_write_bytes += n;
        *(uint32_t*)((uint8_t*)user_map_ + WRITE_H2C_DDR_PTR) = h2c_seek_ + ONCEBYTES;
        msync((void*)user_map_, MAP_SIZE, MS_ASYNC);
        h2c_seek_ += ONCEBYTES;
        already_write_bytes += ONCEBYTES;
        temp_count--;
    }
    if(expect_write_bytes != actual_write_bytes) {
        LOG_ERROR << "PCIe write error: expected write "
                << expect_write_bytes
                << " bytes but actual write " 
                << actual_write_bytes
                <<  " bytes"; 
    }
}

void PCIeLoopThread::GetDDRData(size_t start, size_t size) {
    ::lseek(c2h_handler_, start, SEEK_SET);
    char buf[size];
    ::read(c2h_handler_, buf, size);
    LOG_TRACE << "Get ddr size: " << size;
}

void PCIeLoopThread::SetWriteDDRPtr(uint start, uint size){
    write_start_ptr_ = start;
    write_end_ptr_ = start + size;
    h2c_seek_ = write_start_ptr_;
}