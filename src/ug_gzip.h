#ifndef _UG_GZIP_H
#define _UG_GZIP_H

#define WINSIZE 32768U          /* sliding window size */
#define CHUNK 16384             /* file input buffer size */

int build_gz_index(build_idx_context_t *);
#endif
