#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <time.h>

typedef struct request_t{
    char **buf;
    int lines;
    time_t time;
    char* session;

    struct request_t* next; //for linking
    struct request_t* prev;
}request_t;

request_t* alloc_request();
void free_request(request_t* r);
void add_to_request(request_t* req, char* line);
#endif //__REQUEST_H__
