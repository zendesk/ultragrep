#ifndef __JSON_REQ_H__
#define __JSON_REQ_H__
#include "req_matcher.h"
#include "pcre.h"

typedef struct KVpair
{
    char * key;
    pcre * value;
    struct KVpair * next;

} KVpair;


req_matcher_t *json_req_matcher(on_req fn1, on_err fn2, void *arg);
void handle_json_request(request_t * req, void *cxt_arg);
int add_key_value(char* key_value, void* ctx);
void cleanup_request(void* cxt);

#endif
