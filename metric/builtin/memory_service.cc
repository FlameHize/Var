#include "metric/builtin/memory_service.h"
#include "metric/builtin/tcmalloc_extension.h"

namespace var {

static inline void get_tcmalloc_num_prop(MallocExtension* malloc_ext,
                                         const char* prop_name,
                                         net::BufferStream& os) {
    size_t value;
    if(malloc_ext->GetNumericProperty(prop_name, &value)) {
       os << prop_name << ": " << value << "\n"; 
    }
}

static void get_tcmalloc_memory_info(net::BufferStream& os) {
    MallocExtension* malloc_ext = MallocExtension::instance();
    os << "------------------------------------------------\n";
    get_tcmalloc_num_prop(malloc_ext, "generic.total_physical_bytes", os);
    get_tcmalloc_num_prop(malloc_ext, "generic.current_allocated_bytes", os);
    get_tcmalloc_num_prop(malloc_ext, "generic.heap_size", os);
    get_tcmalloc_num_prop(malloc_ext, "tcmalloc.current_total_thread_cache_bytes", os);
    get_tcmalloc_num_prop(malloc_ext, "tcmalloc.central_cache_free_bytes", os);
    get_tcmalloc_num_prop(malloc_ext, "tcmalloc.transfer_cache_free_bytes", os);
    get_tcmalloc_num_prop(malloc_ext, "tcmalloc.thread_cache_free_bytes", os);
    get_tcmalloc_num_prop(malloc_ext, "tcmalloc.pageheap_free_bytes", os);
    get_tcmalloc_num_prop(malloc_ext, "tcmalloc.pageheap_unmapped_bytes", os);

    int32_t len = 64 * 1024;
    std::unique_ptr<char[]> buf(new char[len]);
    malloc_ext->GetStats(buf.get(), len);
    os << buf.get();
}

MemoryService::MemoryService() {
    AddMethod("/memory", std::bind(&MemoryService::default_method,
        this, std::placeholders::_1, std::placeholders::_2));
}

void MemoryService::default_method(net::HttpRequest* request,
                                   net::HttpResponse* response) {
    response->header().set_content_type("text/plain");
    net::BufferStream os;
    if(IsTCMallocEnabled()) {
        get_tcmalloc_memory_info(os);
    }
    else {
        os << "tcmalloc is not enabled";
        response->header().set_status_code(net::HTTP_STATUS_FORBIDDEN);
    }
    response->set_body(os);
    return;
}

} // end namespace var