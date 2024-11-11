#include "pcie/PCIeServer.h"

#include <filesystem>
#include <regex>

using namespace var;
using namespace var::net;

PCIeServer::PCIeServer(size_t channel_size)
    : channel_size_(channel_size)
    , enable_xvc_(false) {
    for(size_t i = 0; i < XDMACHANNELNUM; ++i) {
        user_handler_[i] = 0;
        c2h_handler_[i] = 0;
        h2c_handler_[i] = 0;
        h2c_seek_[i] = 0;
        user_map_[i] = nullptr;
        loop_thread_[i] = nullptr;
    }
}

PCIeServer::~PCIeServer() {
    for(size_t i = 0; i < channel_size_; ++i) {
        if(loop_thread_[i] != nullptr) {
            loop_thread_[i]->Quit();
        }
    }
    for(size_t i = 0; i < xvc_server_list_.size(); ++i) {
        delete xvc_server_list_[i];
        xvc_server_list_[i] = nullptr;
    }
}

bool PCIeServer::SetCardId(const std::string& card_name, int card_id) {
    bool find_flag = false;
    size_t channel = 0;
    for(size_t i = 0; i < channel_size_; ++i) {
        int id = *(uint32_t*)((uint8_t*)user_map_[i] + READ_SOFTWARE_ID);
        if(id == card_id) {
            find_flag = true;
            channel = i;
            break;
        }
    }
    if(!find_flag) {
        LOG_ERROR << "input card " << card_id << " is not correct with fpga setting, please check FPGA bar memory flag";
        return false;
    }

    // update.
    name_to_card_id_[card_name] = card_id;
    name_to_channel_id_[card_name] = channel;

    // Set device id as tcp port to construct xvc server.
    //net::InetAddress xvc_server_addr("0.0.0.0", card_id);
    //XvcServer* server = new XvcServer(xvc_server_addr, xvc_handler_[channel]);
    //xvc_server_list_.push_back(server);
    return true;
}

bool PCIeServer::SetCardDDRInfo(const std::string& card_name, 
                            int ddr_index, 
                            int ddr_start,
                            int ddr_size) {
    // init ddr size design memory.
    auto iter = name_to_channel_id_.find(card_name);
    if(iter == name_to_channel_id_.end()) {
        LOG_ERROR << "card " << card_name << " not found, please check SetCardId() config";
        return false;
    }
    int channel = iter->second;

    if(ddr_index > 7) {
        LOG_INFO << "Server max ddr memory size is 6, please input num < 6";
        return false;
    }

    ddr_size_design_[channel][ddr_index][0] = ddr_start;
    ddr_size_design_[channel][ddr_index][1] = ddr_size;
    return true;  
}

bool PCIeServer::SetCardDataProcessFunc(const std::string& card_name, const PCIeLoopThread::FileProcessCallback& cb) {
    auto iter = name_to_channel_id_.find(card_name);
    if(iter == name_to_channel_id_.end()) {
        LOG_ERROR << "card " << card_name << " not found, please cjecl SetCardOd() config";
        return false;
    }
    if(name_to_process_func_.find(card_name) != name_to_process_func_.end()) {
        LOG_WARN << "Repeat set data process func with card: " << card_name;
    }
    name_to_process_func_[card_name] = cb;
    return true;
}

void PCIeServer::Start() {
    // Check ddr size design settings.
    bool ddr_flag = true;
    for(size_t i = 0; i < channel_size_; ++i) {
        for(size_t j = 0; j < DDRSPLITSIZE; ++j) {
            uint ddr_start = ddr_size_design_[i][j][0];
            uint ddr_size = ddr_size_design_[i][j][1];
            uint ddr_expect_next_start = ddr_start + ddr_size;
            uint ddr_actual_next_start = ddr_size_design_[i][j + 1][0];
            if(ddr_expect_next_start != ddr_actual_next_start) {
                LOG_ERROR << "Channel: " << i 
                          << " size: " << j
                          << " memory start is " << ddr_start
                          << " memory size is " << ddr_size
                          << " expect next ddr size start is " << ddr_expect_next_start
                          << " but actual set next ddr size is " << ddr_actual_next_start;
                ddr_flag = false; 
            }
        }
    }
    if(!ddr_flag) {
        LOG_ERROR << "DDR size design setting error, Pleasse check PCIeServer::SetDDRInfo() func";
        return;
    }

    for(size_t i = 0; i < channel_size_; ++i) {
        // Reset dma. 
        uint reg_value = 0;
        reg_value = *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_RESET_REG);
        reg_value &= 0xe;
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_RESET_REG) = reg_value;
        reg_value |= 0b1111;
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_RESET_REG) = reg_value;
        ::msync((void*)user_map_[i], MAP_SIZE, MS_ASYNC);

        // Alloc ddr memory.
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_START_1) = ddr_size_design_[i][0][0];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_SIZE_1)  = ddr_size_design_[i][0][1];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_START_2) = ddr_size_design_[i][1][0];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_SIZE_2)  = ddr_size_design_[i][1][1];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_START_3) = ddr_size_design_[i][2][0];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_SIZE_3)  = ddr_size_design_[i][2][1];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_START_4) = ddr_size_design_[i][3][0];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_SIZE_4)  = ddr_size_design_[i][3][1];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_START_5) = ddr_size_design_[i][4][0];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_SIZE_5)  = ddr_size_design_[i][4][1];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_START_6) = ddr_size_design_[i][5][0];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_C2H_DDR_SIZE_6)  = ddr_size_design_[i][5][1];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_H2C_DDR_START)   = ddr_size_design_[i][6][0];
        *(uint32_t*)((uint8_t*)user_map_[i] + WRITE_H2C_DDR_SIZE)    = ddr_size_design_[i][6][1];
        ::msync((void*)user_map_[i], MAP_SIZE, MS_ASYNC);

        // Create PCIe Recv Thread.
        std::string thread_name;
        for(auto it = name_to_channel_id_.begin(); it != name_to_channel_id_.end(); ++it) {
            if(it->second == i) {
                thread_name = it->first;
                break;
            }
        }
        loop_thread_[i] = new PCIeLoopThread(h2c_handler_[i],
                                             c2h_handler_[i],
                                             user_map_[i],
                                             &ddr_size_design_[i][0][0],
                                             thread_name);                                  
        loop_thread_[i]->SetWriteDDRPtr(ddr_size_design_[i][6][0], ddr_size_design_[i][6][1]);
        
        auto it = name_to_process_func_.find(thread_name);
        if(it == name_to_process_func_.end()) {
            loop_thread_[i]->SetFileProcessCallback(std::bind(&PCIeServer::OnDefaultPCIeFileData,
                                                    this, 
                                                    std::placeholders::_1,
                                                    std::placeholders::_2,
                                                    std::placeholders::_3));
        }
        else {
            loop_thread_[i]->SetFileProcessCallback(it->second);
        }
        
        loop_thread_[i]->Start();
        int affinity = loop_thread_[i]->SetAffinity(i);
        if(affinity != 0) {
            LOG_INFO << "PCIe thread" << " affinity with cpu " << i << " failed";
        }
    }

    for(size_t i = 0; i < channel_size_; ++i) {
        std::string thread_name;
        for(auto it = name_to_channel_id_.begin(); it != name_to_channel_id_.end(); ++it) {
            if(it->second == i) {
                thread_name = it->first;
                break;
            }
        }

        // fix: Init call 10th write func.
        for(size_t j = 0; j < 10; ++j) {
            //Warning: must write 64 bytes data to FPGA but not use.
            char data[ONCEBYTES];
            WriteToFPGA(thread_name, 0, data, ONCEBYTES);
        }
    }

    // Start xvc server.
    if(enable_xvc_) {
        for(size_t i = 0; i < xvc_server_list_.size(); ++i) {
            xvc_server_list_[i]->Start();
        }
    }
}

bool PCIeServer::OpenXDMADriver(const std::string& driver_path) {
    // Check driver.
    std::filesystem::path fs_path(driver_path);

    if(!std::filesystem::exists(fs_path) || 
        !std::filesystem::is_directory(fs_path)) {
        LOG_ERROR << "PCIe XDMA Driver path not exists";
        return false;
    }
    std::regex regex_pattern("xdma*user");
    for(const auto& entry : std::filesystem::directory_iterator(fs_path)) {
        if(entry.is_regular_file() && 
            std::regex_match(entry.path().filename().string(), regex_pattern)) {
            ++channel_size_;
        }
    }
    if(!channel_size_) {
        LOG_ERROR << "Not found any xdma user fd, please check the dirver path";
        return false;
    }

    if(channel_size_ > XDMACHANNELNUM) {
        LOG_WARN << "Found XDMA device num is " << channel_size_
                 << ", overhead the max channel limit: " << XDMACHANNELNUM;
    }

    for(size_t i = 0; i < channel_size_; ++i) {
        // Open device fd.
        std::string channel_str = std::string("/dev/xdma") + std::to_string(i);
        std::string user_str = channel_str + std::string("_user");
        std::string h2c_str = channel_str + std::string("_h2c_0");
        std::string c2h_str = channel_str + std::string("_c2h_0");
        std::string xvc_str = channel_str + std::string("_xvc");

        user_handler_[i] = ::open(user_str.c_str(), O_RDWR | O_SYNC);
        if(user_handler_[i] < 0) {
            LOG_ERROR << "Open PCIe user handler: " << user_str << " failed";
            return false;
        }

        h2c_handler_[i] = ::open(h2c_str.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0666);
        if(h2c_handler_[i] < 0) {
            LOG_ERROR << "Open PCIe h2c handler: " << h2c_str << " failed";
            return false;
        }

        c2h_handler_[i] = ::open(c2h_str.c_str(), O_RDWR);
        if(c2h_handler_[i] < 0) {
            LOG_ERROR << "Open PCIe c2h handler: " << c2h_str << " failed";
            return false;
        }

        xvc_handler_[i] = ::open(xvc_str.c_str(), O_RDWR | O_SYNC);
        if(xvc_handler_[i] < 0) {
            LOG_ERROR << "Open PCIe xvc handler: " << xvc_str << " failed";
            return false;
        }

        // FPGA bar is first 64b in mmap memory.
        user_map_[i] = ::mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, user_handler_[i], 0);
        if(user_map_[i] == (void*)-1) {
            LOG_ERROR << "Open PCIe mmap memory failed";
            return false;
        }
    }
    return true;
}

void PCIeServer::OnDefaultPCIeFileData(const char* data, size_t len, uint type) {
    LOG_INFO << "len: " << len << " type: " << type;
}

void PCIeServer::WriteToFPGA(const std::string& card_name, uint type, const char* data, size_t len) {
    auto iter = name_to_channel_id_.find(card_name);
    if(iter == name_to_channel_id_.end()) {
        LOG_ERROR << card_name << "'s xdma channel has not found";
        return;

    }
    int channel = iter->second;
    PCIeLoopThread* thread = loop_thread_[channel];
    if(thread != nullptr) {
        thread->WriteToFPGA(data, len, type);
    }
}