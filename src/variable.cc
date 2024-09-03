#include "variable.h"
#include "common.h"
#include "net/base/Logging.h"
#include "net/base/Mutex.h"
#include <sstream>

namespace var {

class VarEntry {
public:
    VarEntry() : var(nullptr), display_filter(DISPLAY_ON_ALL) {}
    Variable* var;
    DisplayFilter display_filter;
};

typedef std::unordered_map<std::string, VarEntry> VarMap;

struct VarMapWithLock : public VarMap {
    MutexLock mutex;
    VarMapWithLock() {
        reserve(1024);
    }
};

// We have to initialize global map on need because bvar is possibly used
// before main().
static pthread_once_t s_var_maps_once = PTHREAD_ONCE_INIT;
static VarMapWithLock* s_var_maps = nullptr;
const size_t SUB_MAP_COUNT = 32;

static void init_var_maps() {
    // It's probably slow to initialize all sub maps, but user ofen expose
    // variables before user. So this should not be an issue to users.
    s_var_maps = new VarMapWithLock[SUB_MAP_COUNT];
}

inline size_t sub_map_index(const std::string& str) {
    if(str.empty()) {
        return 0;
    }
    size_t h = 0;
    // we're assume that str is ended with '\0', which may not be in general.
    for(const char* p = str.c_str(); *p; ++p) {
        h = h * 5 + *p;
    }
    return h & (SUB_MAP_COUNT - 1);
}

inline VarMapWithLock* get_var_maps() {
    pthread_once(&s_var_maps_once, init_var_maps);
    return s_var_maps;
}

inline VarMapWithLock& get_var_map(const std::string& name) {
    VarMapWithLock& var_map = get_var_maps()[sub_map_index(name)];
    return var_map;
}

Variable::~Variable() {
    if(!is_hidden()) {
        LOG_ERROR << "Subclass of Variable MUST call hide() manually in their"
            " dtors to avoid displaying a variable that is destructing";
    }
}

int Variable::expose_impl(const std::string& prefix,
                          const std::string& name,
                          DisplayFilter display_filter) {
    if(name.empty()) {
        LOG_ERROR << "Parameter[name] is empty";
        return -1;
    }
    // NOTE: It's impossible to atomically erase from a submap and insert into
    // another submap without a global lock. When the to-be-exposed name
    // already exists, there's a chance that we can't insert back previous
    // name. But it should be generally because users are unlikely to 
    // expose a variable more than once and calls to expose() are unlikely
    // to contend heavily.
    // Remove previous pointer from the map if needed.
    hide();

    // Build the name.
    _name.clear();
    _name.reserve((prefix.size() + name.size()) * 5 / 4);
    if(!prefix.empty()) {
        to_underscored_name(&_name, prefix);
        if(!_name.empty() && back_char(_name) != '_') {
            _name.push_back('_');
        }
    }
    to_underscored_name(&_name, name);

    VarMapWithLock& var_map = get_var_map(_name);
    {
        MutexLockGuard lock(var_map.mutex);
        auto iter = var_map.find(_name);
        if(iter == var_map.end()) {
            VarEntry entry;
            entry.var = this;
            entry.display_filter = display_filter;
            var_map[_name] = entry;
            return 0;
        }
    }
    LOG_ERROR << "Already exposed '" << _name << "' whose value is '"
              << describe_exposed(_name) << "'\'";
    _name.clear();
    return -1;
}

bool Variable::is_hidden() const {
    return _name.empty();
}

bool Variable::hide() {
    if(_name.empty()) {
        return false;
    }
    VarMapWithLock& var_map = get_var_map(_name);
    MutexLockGuard lock(var_map.mutex);
    auto iter = var_map.find(_name);
    if(iter != var_map.end()) {
        var_map.erase(_name);
    }
    else {
        LOG_WARN << "'" << _name << "' must exist";
    }
    _name.clear();
    return true;
}

void Variable::list_exposed(std::vector<std::string>* names, DisplayFilter display_filter) {
    if(!names) {
        return;
    }
    names->clear();
    if(names->capacity() < 32) {
        names->reserve(count_exposed());
    }
    VarMapWithLock* var_maps = get_var_maps();
    for(size_t i = 0; i < SUB_MAP_COUNT; ++i) {
        VarMapWithLock& var_map = var_maps[i];
        MutexLockGuard lock(var_map.mutex);
        for(VarMap::const_iterator it = var_map.begin(); it != var_map.end(); ++it) {
            if(it->second.display_filter & display_filter) {
                names->push_back(it->first);
            }
        }
    }
}

size_t Variable::count_exposed() {
    size_t n = 0;
    VarMapWithLock* var_maps = get_var_maps();
    for(size_t i = 0; i < SUB_MAP_COUNT; ++i) {
        n += var_maps[i].size();
    }
    return n;
}

int Variable::describe_exposed(const std::string& name, std::ostream& os,
                               bool quote_string, DisplayFilter display_filter) {
    VarMapWithLock& var_map = get_var_map(name);
    MutexLockGuard lock(var_map.mutex);
    auto iter = var_map.find(name);
    if(iter == var_map.end()) {
        return -1;
    }
    VarEntry* entry = &iter->second;
    if(!(display_filter & entry->display_filter)) {
        return -1;
    }
    entry->var->describe(os, quote_string);
    return 0;
}

std::string Variable::describe_exposed(const std::string& name, bool quote_string,
                                       DisplayFilter display_filter) {
    std::ostringstream oss;
    if (describe_exposed(name, oss, quote_string, display_filter) == 0) {
        return oss.str();
    }
    return std::string();
}

std::string Variable::get_description() const {
    std::ostringstream os;
    describe(os, false);
    return os.str();
}

int Variable::describe_series_exposed(const std::string& name, std::ostream& os) {
    VarMapWithLock& var_map = get_var_map(name);
    MutexLockGuard lock(var_map.mutex);
    auto iter = var_map.find(name);
    if (iter == var_map.end()) {
        return -1;
    }
    VarEntry* entry = &iter->second;
    return entry->var->describe_series(os);
}

DumpOptions::DumpOptions()
    : quote_string(true)
    , question_mark('?')
    , display_filter(DISPLAY_ON_PLAIN_TEXT)
{}

int Variable::dump_exposed(Dumper* dumper, const DumpOptions* options) {
    if(!dumper) {
        LOG_ERROR << "Parameter[dumper] is NULL";
        return -1;
    }
    DumpOptions opt;
    if(options) {
        opt = *options;
    }
    CharArrayStreamBuf streambuf;
    std::ostream os(&streambuf);
    int count = 0;
    WildcardMatcher black_matcher(opt.black_wildcards,
                                  opt.question_mark,
                                  false);
    WildcardMatcher white_matcher(opt.white_wildcards,
                                  opt.question_mark,
                                  true);
    if(white_matcher.wildcards().empty() && !white_matcher.exact_names().empty()) {
        for(std::set<std::string>::const_iterator it = white_matcher.exact_names().begin(); 
            it!= white_matcher.exact_names().end(); ++it) {
            const std::string& name = *it;
            if(!black_matcher.match(name)) {
                if(Variable::describe_exposed(
                    name, os, opt.quote_string, opt.display_filter) != 0) {
                    continue;
                }
                ///@todo log
                if(!dumper->dump(name, streambuf.data())) {
                    return -1;
                }
                streambuf.reset();
                ++count;
            }
        }
    }
    else {
        // Have to iterate all variables.
        std::vector<std::string> varnames;
        Variable::list_exposed(&varnames, opt.display_filter);
        // Sort the names to make them more readable.
        std::sort(varnames.begin(), varnames.end());
        for(std::vector<std::string>::const_iterator 
                it = varnames.begin(); it != varnames.end(); ++it) {
            const std::string& name = *it;
            if(white_matcher.match(name) && !black_matcher.match(name)) {
                if(Variable::describe_exposed(
                    name, os, opt.quote_string, opt.display_filter) != 0) {
                    continue;
                }
                ///@todo log
                if(!dumper->dump(name, streambuf.data())) {
                    return -1;
                }
                streambuf.reset();
                ++count;
            }
        }
    }
    return count;
}

void to_underscored_name(std::string* name, const std::string& src) {
    name->reserve(name->size() + src.size() + 8/*just guess*/);
    for (const char* p = src.data(); p != src.data() + src.size(); ++p) {
        if (isalpha(*p)) {
            if (*p < 'a') { // upper cases
                if (p != src.data() && !isupper(p[-1]) &&
                    back_char(*name) != '_') {
                    name->push_back('_');
                }
                name->push_back(*p - 'A' + 'a');
            } else {
                name->push_back(*p);
            }
        } else if (isdigit(*p)) {
            name->push_back(*p);
        } else if (name->empty() || back_char(*name) != '_') {
            name->push_back('_');
        }
    }
}

} // end namespace var