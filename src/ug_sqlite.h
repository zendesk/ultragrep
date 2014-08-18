#ifndef _UG_SQLITE_H
#define _UG_SQLITE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sqlite3.h>

sqlite3 *ug_sqlite_get_db(char *log_fname, int rw);
int ug_sqlite_index_timestamp(sqlite3 *db, uint64_t ts, uint64_t offset);
int ug_sqlite_index_gzip_header(sqlite3 *db, uint64_t raw_offset, uint64_t gz_offset, unsigned char *blob, int blob_len);
off_t ug_sqlite_get_ts_offset(sqlite3 *db, uint64_t ts);
int ug_sqlite_get_gzip_info(sqlite3 *db, uint64_t raw_offset, uint64_t *gz_offset, unsigned char **gz_header);
#endif
