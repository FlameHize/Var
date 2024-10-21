#include "src/builtin/inside_cmd_status_user.h"
#include "src/util/tinyxml2.h"
#include "net/base/Logging.h"

using namespace tinyxml2;

namespace var {

const size_t kFixedChipBytes = 256;

InsideCmdStatusUser::InsideCmdStatusUser(const std::string& user_name,
                                         size_t user_id)
    : _user_name(user_name)
    , _user_id(user_id) {
}

int InsideCmdStatusUser::parse(const std::string& path) {
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

    size_t chip_count = _user_id;
    for(XMLElement* elem = field_num_element->FirstChildElement();
        elem; elem = elem->NextSiblingElement()) {

        ChipInfo chip_info;
        chip_info.label = elem->Attribute("Label");
        chip_info.key_num = elem->IntAttribute("KeyNum");
        chip_info.field_byte = elem->IntAttribute("FieldByte");
        chip_info.index = chip_count++;

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
    "}\n"  
    "th {\n"  
    "    background-color: #f2f2f2;\n"  
    "}\n"  
    "tr {\n"
    "    height: 40px;\n"
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
    "</style>\n"; 

    os << "<table>\n"  
    "    <thead>\n"  
    "        <tr>\n"; 
    for(size_t i = 0; i < _chip_info_list.size(); ++i) {
        const ChipInfo& chip = _chip_info_list.at(i);
        os << "<th onclick=\"showContent('" << chip.label 
           << "')\">" << chip.label << "</th>\n"; 
    } 
    os << "  </tr>\n"  
    "    </thead>\n"  
    "</table>\n";  
  
    os << "<div id=\"content-container\">\n";
    for(size_t i = 0; i < _chip_info_list.size(); ++i) {
        ChipInfo& chip = _chip_info_list.at(i);
        os << "<div id=\"" << chip.label << "\"";
        os << "class=\"content-section\">\n";
        chip.describe(data, len, os, use_html);
        os << "</div>\n";
    }
    os << "</div>\n"  
    "<script>\n"  
    "    function showContent(pageId) {\n"    
    "        var contentSections = document.querySelectorAll('.content-section');\n"  
    "        contentSections.forEach(function(section) {\n"  
    "            section.classList.remove('active');\n"  
    "        });\n"    
    "        var selectedSection = document.getElementById(pageId);\n"  
    "        if(selectedSection) {\n"  
    "            selectedSection.classList.add('active');\n" 
    "        }\n"  
    "    }\n"    
    "</script>\n";  
}

void ChipInfo::describe(const char* data, size_t len,
                        std::ostream& os, bool use_html) {
    os << "<div class=\"table-container\">\n";
    int rows = 16;
    int count = 0;
    for(size_t i = 0; i < key_info_list.size(); ++i) {
        const KeyInfo& info = key_info_list.at(i);
        if(count == 0) {
            os << "<table>\n";
        }
        os << "<tr><td>\n";
        os << info.name << " : " << std::to_string(info.default_value);
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

} // end namespace var