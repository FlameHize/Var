#include "metric/builtin/inside_cmd_status_user.h"
#include "metric/builtin/common.h"
#include "metric/util/dir_reader_linux.h"

using namespace tinyxml2;

namespace var {

const size_t kFixedChipBytes = 256;

InsideCmdStatusUser::InsideCmdStatusUser(const std::string& user_name,
                                         size_t user_id)
    : _user_name(user_name)
    , _user_id(user_id) {
}

InsideCmdStatusUser::~InsideCmdStatusUser() {
    if(_doc) {
        delete _doc;
        _doc = nullptr;
    }
    for(size_t i = 0; i < _chip_info_list.size(); ++i) {
        ChipInfo* chip = _chip_info_list.at(i);
        if(chip) {
            delete chip;
            chip = nullptr;
        }
    }
    _chip_info_list.clear();
}

int InsideCmdStatusUser::parse(const std::string& path) {
    if(path.empty()) {
        return -1;
    }
    XMLDocument* doc = new XMLDocument;
    XMLError load_result = doc->LoadFile(path.c_str());
    if(load_result != XML_SUCCESS) {
        delete doc;
        doc = nullptr;
        LOG_ERROR << "Failed to load " << path << ", error code: " << load_result;
        return load_result;
    }
    return parse_internal(doc);
}

int InsideCmdStatusUser::parse(const char* data, size_t len) {
    if(!data) {
        return -1;
    }
    XMLDocument* doc = new XMLDocument;
    XMLError load_result = doc->Parse(data, len);
    if(load_result != XML_SUCCESS) {
        delete doc;
        doc = nullptr;
        LOG_ERROR << "Failed to load data" << ", error code: " << load_result;
        return load_result;
    }
    return parse_internal(doc);
}

int InsideCmdStatusUser::parse_internal(XMLDocument* doc) {
    XMLElement* field_num_element = doc->RootElement();
    if(!field_num_element) {
        LOG_ERROR << "Parameter[FieldNum] is NULL";
        return -1;
    }
    _doc = doc;
    int field_num = field_num_element->IntAttribute("FieldNum");
    if(field_num != field_num_element->ChildElementCount()) {
        LOG_WARN << "Setting FieldNum is " << field_num 
                 << " but actual is " << field_num_element->ChildElementCount();
    }

    size_t chip_count = _user_id;
    for(XMLElement* elem = field_num_element->FirstChildElement();
        elem; elem = elem->NextSiblingElement()) {

        ChipInfo* chip_info = new ChipInfo;
        chip_info->xml_element = elem;
        chip_info->owner = (void*)this;
        chip_info->label = elem->Attribute("Label");
        chip_info->key_num = elem->IntAttribute("KeyNum");
        chip_info->field_byte = elem->IntAttribute("FieldByte");
        chip_info->index = chip_count++;

#ifdef DEBUG
        if(chip_info.key_num != elem->ChildElementCount()) {
            LOG_WARN << chip_info.label << " setting KeyNum is " 
                     << chip_info.key_num << " but actual is "
                     << elem->ChildElementCount();
        }
#endif
        size_t value_addr = chip_info->index * kFixedChipBytes;
        for(XMLElement* key_elem = elem->FirstChildElement();
            key_elem; key_elem = key_elem->NextSiblingElement()) {
            KeyInfo* key_info = new KeyInfo;
            key_info->xml_element = key_elem;
            key_info->owner = (void*)chip_info;
            key_info->name = key_elem->FirstChildElement("name")->GetText();
            key_info->type = key_elem->FirstChildElement("type")->IntText();
            key_info->byte = key_elem->FirstChildElement("byte")->IntText();
            key_info->sign =  key_elem->FirstChildElement("sign")->IntText();
            key_info->default_value =  key_elem->FirstChildElement("default")->IntText();
            key_info->enable = key_elem->FirstChildElement("enable")->IntText();
            key_info->unit = key_elem->FirstChildElement("unit")->DoubleText();
            key_info->offset = key_elem->FirstChildElement("offset")->DoubleText();
            key_info->precesion = key_elem->FirstChildElement("precision")->IntText();
            
            const char* dimension = key_elem->FirstChildElement("dimension")->GetText();
            if(dimension) {
                key_info->dimension = dimension;
            }

            XMLElement* list_elem = key_elem->FirstChildElement("list");
            key_info->list_num = std::stoi(list_elem->Attribute("ListNum"));
            for(XMLElement* select_elem = list_elem->FirstChildElement(); 
                select_elem; select_elem = select_elem->NextSiblingElement()) {
                key_info->select_list.push_back(select_elem->GetText());
            }

            // Resolved_addr calculate for inside status.
            key_info->resolved_addr = value_addr;
            value_addr += key_info->byte;

            // Init set value for inside cmd.
            key_info->set_value = -1;

            chip_info->key_info_list.push_back(key_info);
        }
        _chip_info_list.push_back(chip_info);
    }
    return XML_SUCCESS;
}

void InsideCmdStatusUser::describe(const char* data, size_t len,
                                   std::ostream& os, bool cmd_or_status) {
    os << "<style>\n"  
    "table {\n"  
    "    width: 100%;\n"  
    "    border-collapse: collapse;\n"  
    "}\n"  
    "th, td {\n"  
    "    border: 1px solid black;\n"  
    "    padding: 8px;\n"  
    "    text-align: center;\n"  
    "    cursor: pointer;\n"  
    "    justify-content: space-between;\n"
    "    align-items:center;\n"
    "}\n"  
    "th {\n"  
    "    font-size: 14px;\n"
    "    background-color: #f2f2f2;\n"  
    "}\n"  
    "th:hover {\n"
    "    background-color: #999999;\n"
    "}\n"
    "tr {\n"
    "    font-size: 12px;\n"
    "    height: 40px;\n"
    "}\n"
    ".content-tab.current {\n"
    "    background-color: #999999;\n"
    "}\n"
    ".content-section {\n"  
    "    margin-top: 20px;\n"  
    "    padding: 20px;\n"  
    "    border: 1px solid #ccc;\n"  
    "    display: none;\n"  
    "}\n"  
    ".content-section.active {\n"  
    "    display: block;\n"
    "}\n"  
    ".table-container {\n"
    "    display: grid;\n"
    "    grid-template-columns: repeat(auto-fill, minmax(240px, 1fr));\n"
    "    gap: 30px;\n"
    "}\n"
    ".fixed-button {\n"
    "    position: fixed;\n"
    "    bottom: 10px;\n"
    "    right: 10px;\n"
    "    background-color: #007bff;\n"
    "    color: white;\n"
    "    border: none;\n"
    "    border-radius: 5px;\n"
    "    cursor: pointer;\n"
    "    font-size: 18px;\n"
    "    gap: 30px;\n"
    "}\n"
    ".fixed-button:hover {\n"
    "    background-color: #0056b3;\n"
    "}\n"
    "</style>\n"; 

    os << "<table>\n"  
    "    <thead>\n"  
    "        <tr>\n"; 
    for(size_t i = 0; i < _chip_info_list.size(); ++i) {
        const ChipInfo* chip = _chip_info_list.at(i);
        os << "<th onclick=\"showContent('" << chip->label 
           << "'); changeColor(this)\" class=\"content-tab\">" 
           << chip->label << "</th>\n"; 
    } 
    os << "  </tr>\n"  
    "    </thead>\n"  
    "</table>\n";  
  
    os << "<div id=\"content-container\">\n";
    for(size_t i = 0; i < _chip_info_list.size(); ++i) {
        ChipInfo* chip = _chip_info_list.at(i);
        os << "<div id=\"" << chip->label << "\"";
        os << "class=\"content-section\"";
        os << "data-index=\"" << chip->index << "\">\n";
        chip->describe(data, len, os, cmd_or_status);
        os << "</div>\n";
    }
    os << "</div>\n"  
    "<script>\n" 
    "    var timerId = {}\n" 
    "    function showContent(pageId) {\n"    
    "        var contentSections = document.querySelectorAll('.content-section');\n"  
    "        contentSections.forEach(function(section) {\n"  
    "            section.classList.remove('active');\n"  
    "        });\n"    
    "        var selectedSection = document.getElementById(pageId);\n"  
    "        if(selectedSection) {\n"  
    "            selectedSection.classList.add('active');\n" 
    "            var chipIndex = selectedSection.getAttribute(\"data-index\");\n"
    "            if(timerId) {\n"
    "               clearInterval(timerId);\n"
    "            }\n"
    "            timerId = setInterval(function(){\n"
    "            $.ajax({\n"
    "               url: \"/" << (cmd_or_status ? "inside_cmd" : "inside_status") << "/show_chip_info?chipIndex=\" + chipIndex,\n"
    "               type: \"GET\",\n"
    "               dataType: \"html\",\n"
    "               success: function(data) {\n"
    "                   selectedSection.innerHTML = data;\n"
    "               }\n"
    "            });\n"
    "            }, 1000);\n"
    "            if(" << cmd_or_status << ") {\n"
    "               clearInterval(timerId);\n"
    "            }\n"
    "        }\n"  
    "    }\n"    
    "   function changeColor(element) {\n"
    "       var contentTabs = document.querySelectorAll('.content-tab');\n"
    "       contentTabs.forEach(function(tab) {\n"
    "           tab.classList.remove('current');\n"
    "       });\n" 
    "       element.classList.add('current');\n"
    "   }\n"
    "</script>\n";  

    if(cmd_or_status) {
        os << "<button class=\"fixed-button\" onclick=\"sendCommand()\">发送</button>\n"
        "<script>\n"
        "   function sendCommand() {\n"
        "       $.ajax({\n"
        "       url: '/inside_cmd/send',\n"
        "       type: \"POST\",\n"
        "       data: JSON.stringify({\n"
        "           userName: \"" << name() << "\"\n"
        "       }),\n"
        "       processData: false,\n"
        "       contentType: false,\n"
        "       success: function(response) {\n"
        "           alert('用户" << name() << ": 内部指令下发成功');\n"
        "       },\n"
        "       error: function(error) {\n"
        "           var errStr = \"指令下发失败: \" + error.responseText;"
        "           alert(errStr);\n"
        "       }\n"
        "       });\n"
        "   }\n"
        "   function updateItem(userName, chipIndex, itemName, setValue) {\n"
        "       $.ajax({\n"
        "       url: '/inside_cmd/update_chip_info',\n"
        "       type: \"POST\",\n"
        "       data: JSON.stringify({\n"
        "           userName: userName,\n"
        "           chipIndex: chipIndex,\n"
        "           itemName: itemName,\n"
        "           setValue: setValue\n"
        "       }),\n"
        "       processData: false,\n"
        "       contentType: false,\n"
        "       success: function(response) {\n"
        "       },\n"
        "       error: function(error) {\n"
        "           var errStr = \"当前指令数据更新失败: \" + error.responseText;"
        "           alert(errStr);\n"
        "       }\n"
        "       });\n"
        "   }\n"
        "   function getTextValue(element) {\n"
        "       if(element.value < 0) {\n"
        "           alert('仅允许非负数输入');\n"
        "           element.value = element.dataset.lastVaildValue;\n"
        "       }\n"
        "       else {\n"
        "           element.dataset.lastVaildValue = element.value;\n"
        "           var userName = element.dataset.userName;\n"
        "           var chipIndex = element.dataset.chipIndex;\n"
        "           var itemName = element.dataset.itemName;\n"
        "           var setValue = element.value.toString();\n"
        "           updateItem(userName, chipIndex, itemName, setValue);\n"
        "       }\n"
        "   }\n"
        "   function getSelectValue(element) {\n"
        "       var userName = element.dataset.userName;\n"
        "       var chipIndex = element.dataset.chipIndex;\n"
        "       var itemName = element.dataset.itemName;\n"
        "       var setValue = element.selectedIndex.toString();\n"
        "       updateItem(userName, chipIndex, itemName, setValue);\n"
        "   }\n"
        "</script>\n";
    }
}

ChipInfo::ChipInfo() {
}

ChipInfo::~ChipInfo() {
    for(size_t i = 0; i < key_info_list.size(); ++i) {
        KeyInfo* key = key_info_list.at(i);
        if(key) {
            delete key;
            key = nullptr;
        }
    }
    key_info_list.clear();
}

void ChipInfo::describe(const char* data, size_t len,
                        std::ostream& os, bool cmd_or_status) {
    os << "<div class=\"table-container\">\n";
    int rows = 16;
    int count = 0;
    for(size_t i = 0; i < key_info_list.size(); ++i) {
        KeyInfo* info = key_info_list.at(i);
        if(count == 0) {
            os << "<table>\n";
        }
        os << "<tr><td>\n";
        info->describe(data, len, os, cmd_or_status);
        os << "</td></tr>\n"; 
        ++count;
        if(count == rows) {
            count = 0;
            os << "</table>\n";
        }
    }
    if(count != 0) {
        os << "</table>\n";
    }
    os << "</div>\n";
}

void KeyInfo::describe(const char* data, size_t len,
                       std::ostream& os, bool cmd_or_status) {
    if(!cmd_or_status) {
        if(!data || len == 0) {
            os << name << " : " << std::to_string(default_value) << dimension;
            return;
        }
        if(resolved_addr + byte > len) {
            os << name << " : " << std::to_string(default_value) << dimension;
            return;
        }
    }

    if(cmd_or_status) {
        std::string user_name;
        std::string chip_index;
        if(owner) {
            ChipInfo* chip = static_cast<ChipInfo*>(owner);
            chip_index = std::to_string(chip->index);
            if(chip->owner) {
                InsideCmdStatusUser* user = static_cast<InsideCmdStatusUser*>(chip->owner);
                user_name = user->name();
            }
        }
        os << "<label style=\"text-align: left;\">" << name << ": " << "</label>\n";
        if(type) {
            // Display priority: set_value > default_value.
            std::string value;
            value = (set_value != -1 ? std::to_string(set_value) : std::to_string(default_value));
            os << "<input type=\"number\" style=\"width:100px; font-size:12px; text-align: center;\" " 
               << "min=\"0\" "
               << "data-user-name=\"" << user_name << "\" "
               << "data-chip-index=\"" << chip_index << "\" "
               << "data-item-name=\"" << name << "\" " 
               << "data-last-vaild-value=\"" << value << "\" "
               << "class=\"cmd-text-input\" onblur=\"getTextValue(this)\" "
               << "value=\"" << value << "\">\n";
        }
        else {
            os << "<select style=\"width:100px; font-size:12px; text-align: center;\" " 
               << "data-user-name=\"" << user_name << "\" "
               << "data-chip-index=\"" << chip_index << "\" "
               << "data-item-name=\"" << name << "\" " 
               << "class=\"cmd-select-input\" onblur=\"getSelectValue(this)\">\n";
            for(size_t i = 0; i < select_list.size(); ++i) {
                os << "<option";
                if(set_value != -1) {
                    if(i == static_cast<size_t>(set_value)) {
                        os << " selected>";
                    }
                    else {
                        os << ">";
                    }
                }
                else {
                    if(i == static_cast<size_t>(default_value)) {
                        os << " selected>";
                    }
                    else {
                        os << ">";
                    }
                }
                os << select_list.at(i) << "</option>\n";
            }
            os << "</select>\n";
        }
    }
    else {
        double resolved_value = 0;
        switch(byte) {
        case 1: {
            if(sign) {
                int8_t value = 0;
                memcpy((char*)&value, data + resolved_addr, sizeof(value));
                resolved_value = value;
            }
            else {
                uint8_t value = 0;
                memcpy((char*)&value, data + resolved_addr, sizeof(value));
                resolved_value = value;
            }
            break;
        }
        case 2: {
            if(sign) {
                int16_t value = 0;
                memcpy((char*)&value, data + resolved_addr, sizeof(value));
                resolved_value = value;
            }
            else {
                uint16_t value = 0;
                memcpy((char*)&value, data + resolved_addr, sizeof(value));
                resolved_value = value;
            }
            break;
        }
        case 4: {
            if(sign) {
                int32_t value = 0;
                memcpy((char*)&value, data + resolved_addr, sizeof(value));
                resolved_value = value;
            }
            else {
                uint32_t value = 0;
                memcpy((char*)&value, data + resolved_addr, sizeof(value));
                resolved_value = value;
            }
            break;
        }
        case 8: {
            if(sign) {
                int64_t value = 0;
                memcpy((char*)&value, data + resolved_addr, sizeof(value));
                resolved_value = value;
            }
            else {
                uint64_t value = 0;
                memcpy((char*)&value, data + resolved_addr, sizeof(value));
                resolved_value = value;
            }
            break;
        }
        default:
            break;
        }
        double format_value = offset + unit * resolved_value;
        std::string format_value_str;
        if(enable == 2) {
            format_value_str = decimal_to_binary(format_value);
        }
        else if(enable == 16) {
            format_value_str = decimal_to_hex(format_value);
        }
        else {
            format_value_str = double_to_string(format_value, precesion);
            if(list_num != 0) {
                size_t num = std::stoi(format_value_str);
                if(num >= 0 && num < select_list.size()) {
                    format_value_str = select_list[num];
                }
                else {
                    format_value_str += std::string("(!超出范围!)");
                }
            }
            format_value_str += dimension;
        }
        os << name << " : " << format_value_str;
    }

}

std::string get_first_xml_file(const std::string& dir_path) {
    if(dir_path.empty()) {
        return std::string();
    }
    DIR* dir = opendir(dir_path.c_str());
    if(!dir) {
        LOG_ERROR << "Failed to open directory: " << dir_path;
        return std::string();
    }
    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr) {
        if(entry->d_type == DT_REG && entry->d_name[0] != '.') {
            size_t name_len = strlen(entry->d_name);
            if(name_len >= 4 && 
                strcmp(entry->d_name + name_len - 4, ".xml") == 0) {
                std::string file_path = dir_path + '/';
                file_path += std::string(entry->d_name); 
                closedir(dir);
                return file_path;
            }
        }
    }
    return std::string();
}

} // end namespace var