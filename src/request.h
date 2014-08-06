#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <time.h>

typedef struct request_t {
    char *buf;
    off_t offset;
    time_t time;
} request_t;

void handle_request(request_t * req);
#endif                          //__REQUEST_H__
