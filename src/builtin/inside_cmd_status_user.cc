#include "src/builtin/inside_cmd_status_user.h"
#include "src/util/tinyxml2.h"
#include "net/base/Logging.h"

using namespace tinyxml2;

namespace var {

const size_t kFixedChipBytes = 256;

int InsideCmdStatusUser::parse(const std::string& path,
                               size_t chip_group_index) {
    if(path.empty()) {
        return -1;
    }
    XMLDocument doc;
    XMLError load_result = doc.LoadFile(path.c_str());
    if(load_result != XML_SUCCESS) {
        LOG_ERROR << "Failed to load " << path << ", error code: " << load_result;
        return load_result;
    }
    
    XMLElement* field_num_element = doc.RootElement();
    if(!field_num_element) {
        LOG_ERROR << path << " file's Parameter[FieldNum] is NULL";
        return -1;
    }
    int field_num = field_num_element->IntAttribute("FieldNum");
    if(field_num != field_num_element->ChildElementCount()) {
        LOG_WARN << "Setting FieldNum is " << field_num 
                 << " but actual is " << field_num_element->ChildElementCount();
    }

    _chip_group_index = chip_group_index;
    size_t chip_count = _chip_group_index;
    for(XMLElement* elem = field_num_element->FirstChildElement();
        elem; elem = elem->NextSiblingElement()) {

        ChipInfo chip_info;
        chip_info.label = elem->Attribute("Label");
        chip_info.key_num = elem->IntAttribute("KeyNum");
        chip_info.field_byte = elem->IntAttribute("FieldByte");
        chip_info.index = chip_count++;
        chip_info.GetTabInfo(&_tab_info_list);

#ifdef DEBUG
        if(chip_info.key_num != elem->ChildElementCount()) {
            LOG_WARN << chip_info.label << " setting KeyNum is " 
                     << chip_info.key_num << " but actual is "
                     << elem->ChildElementCount();
        }
#endif
        size_t value_addr = chip_info.index * kFixedChipBytes;
        for(XMLElement* key_elem = elem->FirstChildElement();
            key_elem; key_elem = key_elem->NextSiblingElement()) {
            KeyInfo key_info;
            key_info.name = key_elem->FirstChildElement("name")->GetText();
            key_info.type = key_elem->FirstChildElement("type")->IntText();
            key_info.byte = key_elem->FirstChildElement("byte")->IntText();
            key_info.sign =  key_elem->FirstChildElement("sign")->IntText();
            key_info.default_value =  key_elem->FirstChildElement("default")->IntText();
            key_info.enable = key_elem->FirstChildElement("enable")->IntText();
            key_info.unit = key_elem->FirstChildElement("unit")->DoubleText();
            key_info.offset = key_elem->FirstChildElement("offset")->DoubleText();
            key_info.precesion = key_elem->FirstChildElement("precision")->IntText();
            
            const char* dimension = key_elem->FirstChildElement("dimension")->GetText();
            if(dimension) {
                key_info.dimension = dimension;
            }
            // resolved_addr calculate.
            key_info.resolved_addr = value_addr;
            value_addr += key_info.type;

            chip_info.key_info_list.push_back(key_info);
        }
        _chip_info_list.push_back(chip_info);
    }
    return XML_SUCCESS;
}

void InsideCmdStatusUser::describe(const char* data, size_t len,
                                   std::ostream& os, bool use_html) {
    for(size_t i = 0; i < _tab_info_list.size(); ++i) {
        const TabInfo& info = _tab_info_list[i];
        os << info.tab_name << "\n";
    }
}

} // end namespace var