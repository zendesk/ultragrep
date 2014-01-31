#ifndef __REQ_CHECKER_H__
#define __REQ_CHECKER_H__
//#include "request.h"
#include <sys/types.h>
#include "pcre.h"


int check_request(int lines, char **request, time_t request_time, pcre ** regexps, int num_regexps);

#endif