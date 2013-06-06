#ifndef __REQ_MATCHER_H__
#define __REQ_MATCHER_H__
#include "request.h"
#include <sys/types.h>

typedef void (*on_req)(request_t*, void* arg);
typedef void (*on_err)(char*, ssize_t, void* arg);

typedef struct req_matcher_t{
    int (*process_line)(struct req_matcher_t* base, char* line, ssize_t line_sz, off_t offset);
    void (*stop)(struct req_matcher_t* base);
}req_matcher_t;

#define EOF_REACHED 1
#define STOP_SIGNAL 2

#endif //__REQ_MATCHER_H__
