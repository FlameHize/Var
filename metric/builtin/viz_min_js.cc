#include "metric/builtin/viz_min_js.h"

namespace var {

static net::Buffer* g_viz_min_js_buf = nullptr;

const net::Buffer& viz_min_js_buf() {
    g_viz_min_js_buf = new net::Buffer;
    g_viz_min_js_buf->append(viz_min_js());
    return *g_viz_min_js_buf;
}

const char* viz_min_js() {
return "function Ub(nr){throw nr}var cc=void 0,wc=!0,xc=null,ee=!1;function bk(){return(function(){})}"



}

} // namespace var