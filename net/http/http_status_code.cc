#include <string.h>
#include <pthread.h>
#include "macros.h"
#include "http_status_code.h"

namespace var {
namespace net {
    
struct status_pair {
    int status_code;
    const char* reason_phrase;
};

static status_pair g_status_pairs[] = {
    // Informational 1xx   
    { HTTP_STATUS_CONTINUE,                         "Continue"              },
    { HTTP_STATUS_SWITCHING_PROTOCOLS,              "Switching Protocols"   },

    // Successful 2xx
    { HTTP_STATUS_OK,                               "OK"                    },
    { HTTP_STATUS_CREATED,                          "Created"               },
    { HTTP_STATUS_ACCEPTED,                         "Accepted"              },
    { HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION,    "Non-Authoritative Informational" },
    { HTTP_STATUS_NO_CONTENT,                       "No Content"            },
    { HTTP_STATUS_RESET_CONTENT,                    "Reset Content"         },
    { HTTP_STATUS_PARTIAL_CONTENT,                  "Partial Content"       },

    // Redirection 3xx
    { HTTP_STATUS_MULTIPLE_CHOICES,                 "Multiple Choices"      },
    { HTTP_STATUS_MOVE_PERMANENTLY,                 "Move Permanently"      },
    { HTTP_STATUS_FOUND,                            "Found"                 },
    { HTTP_STATUS_SEE_OTHER,                        "See Other"             },
    { HTTP_STATUS_NOT_MODIFIED,                     "Not Modified"          },
    { HTTP_STATUS_USE_PROXY,                        "Use Proxy"             },
    { HTTP_STATUS_TEMPORARY_REDIRECT,               "Temporary Redirect"    },

    // Client Error 4xx
    { HTTP_STATUS_BAD_REQUEST,                      "Bad Request"           },
    { HTTP_STATUS_UNAUTHORIZED,                     "Unauthorized"          },
    { HTTP_STATUS_PAYMENT_REQUIRED,                 "Payment Required"      },
    { HTTP_STATUS_FORBIDDEN,                        "Forbidden"             },
    { HTTP_STATUS_NOT_FOUND,                        "Not Found"             },
    { HTTP_STATUS_METHOD_NOT_ALLOWED,               "Method Not Allowed"    },
    { HTTP_STATUS_NOT_ACCEPTABLE,                   "Not Acceptable"        },
    { HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED,    "Proxy Authentication Required" },
    { HTTP_STATUS_REQUEST_TIMEOUT,                  "Request Timeout"       },
    { HTTP_STATUS_CONFLICT,                         "Conflict"              },
    { HTTP_STATUS_GONE,                             "Gone"                  },
    { HTTP_STATUS_LENGTH_REQUIRED,                  "Length Required"       },
    { HTTP_STATUS_PRECONDITION_FAILED,              "Precondition Failed"   },
    { HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE,         "Request Entity Too Large" },
    { HTTP_STATUS_REQUEST_URI_TOO_LARG,             "Request-URI Too Long"  },
    { HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE,           "Unsupported Media Type"},
    { HTTP_STATUS_REQUEST_RANGE_NOT_SATISFIABLE,    "Requested Range Not Satisfiable" },
    { HTTP_STATUS_EXPECTATION_FAILED,               "Expectation Failed"    },

    // Server Error 5xx
    { HTTP_STATUS_INTERNAL_SERVER_ERROR,            "Internal Server Error" },
    { HTTP_STATUS_NOT_IMPLEMENTED,                  "Not Implemented"       },
    { HTTP_STATUS_BAD_GATEWAY,                      "Bad Gateway"           },
    { HTTP_STATUS_SERVICE_UNAVAILABLE,              "Service Unavailable"   },
    { HTTP_STATUS_GATEWAY_TIMEOUT,                  "Gateway Timeout"       },
    { HTTP_STATUS_VERSION_NOT_SUPPORTED,            "HTTP Version Not Supported" },
};

static const char* phrases[1024];
static pthread_once_t init_reason_phrase_once = PTHREAD_ONCE_INIT;

static void InitReasonPhrases() {
    memset(phrases, 0, sizeof(phrases));
    for(size_t i = 0; i < get_array_size(g_status_pairs); ++i) {
        if(g_status_pairs[i].status_code >= 0 &&
                g_status_pairs[i].status_code < (int)get_array_size(phrases)) {
            phrases[g_status_pairs[i].status_code] = g_status_pairs[i].reason_phrase;
        }
        else {
            // empty temp;
        }
    }
}

const char* HttpReasonPhrase(int status_code) {
    pthread_once(&init_reason_phrase_once, InitReasonPhrases);
    const char* desc = nullptr;
    if(status_code >= 0 && 
        status_code < (int)get_array_size(phrases)) {
        desc = phrases[status_code];
    }
    return desc;
}

} // end namespace net
} // end namespace var