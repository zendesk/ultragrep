#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "ug_sqlite.h"
#include "ug_index.h"

#define _CHECK_RET(which, ret, db) \
  if ( ret != SQLITE_OK ) { \
    fprintf(stderr, "%s\n", sqlite3_errmsg(db)); \
    return which; \
  }

#define CHECK_RET_PTR(ret, db) _CHECK_RET(NULL, ret, db)
#define CHECK_RET_INT(ret, db) _CHECK_RET(0, ret, db)

static char *ug_sqlite_get_db_fname(char *log_fname, char *ext)
{
    char *tmp, *dir, *index_fname;

    tmp = strdup(log_fname);
    dir = dirname(tmp);

    index_fname = malloc(strlen(dir) + strlen(basename(log_fname)) + strlen("/..") + strlen(ext) + 1);

    sprintf(index_fname, "%s/.%s.%s", dir, basename(log_fname), ext);
    free(tmp);
    return index_fname;
}

sqlite3 *ug_sqlite_get_db(char *log_fname, int rw) {
  int ret;
  char *errmsg;
  sqlite3 *db;
  char *db_fname = ug_sqlite_get_db_fname(log_fname, "db");

  ret = sqlite3_open_v2(db_fname, &db, rw ? SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE : SQLITE_OPEN_READONLY, NULL);
  CHECK_RET_PTR(ret, db);

  sqlite3_exec(db, "CREATE TABLE if not exists ts_indices      (ts primary key, offset)", NULL, NULL, &errmsg);
  sqlite3_exec(db, "CREATE TABLE if not exists gz_indices    (raw_offset primary key, gz_offset, gz_header)", NULL, NULL, &errmsg);
  sqlite3_exec(db, "CREATE TABLE if not exists keyword_indices (keyword, value, offsets)", NULL, NULL, &errmsg);

  return db;
}

int ug_sqlite_index_timestamp(sqlite3 *db, uint64_t ts, uint64_t offset) {
  int ret;
  static sqlite3_stmt *sql = NULL;

  if ( !sql ) {
    ret = sqlite3_prepare_v2(db, "INSERT INTO ts_indices (ts, offset) VALUES (?, ?)", -1, &sql, NULL);
    CHECK_RET_INT(ret, db);
  }

  sqlite3_reset(sql);
  sqlite3_bind_int64(sql, 1, ts);
  sqlite3_bind_int64(sql, 2, offset);

  if ( sqlite3_step(sql) != SQLITE_DONE )
    return 0;

  return 1;
}

int ug_sqlite_index_gzip_header(sqlite3 *db, uint64_t raw_offset, uint64_t gz_offset, unsigned char *blob, int blob_len) {
  int ret;
  static sqlite3_stmt *sql = NULL;

  if ( !sql ) {
    ret = sqlite3_prepare_v2(db, "INSERT INTO gz_indices (raw_offset, gz_offset, gz_header) VALUES (?, ?, ?)", -1, &sql, NULL);
    CHECK_RET_INT(ret, db);
  }

  sqlite3_reset(sql);
  sqlite3_bind_int64(sql, 1, raw_offset);
  sqlite3_bind_int64(sql, 2, gz_offset);
  sqlite3_bind_blob(sql, 3, blob, blob_len, SQLITE_STATIC);

  if ( sqlite3_step(sql) != SQLITE_DONE )
    return 0;

  return 1;
}

off_t ug_sqlite_get_ts_offset(sqlite3 *db, uint64_t ts) {
  uint64_t ret;
  sqlite3_stmt *sql;

  ret = sqlite3_prepare_v2(db, "SELECT offset FROM ts_indices WHERE ts <= ? ORDER BY ts DESC LIMIT 1", -1, &sql, NULL);
  CHECK_RET_INT(ret, db);

  sqlite3_bind_int64(sql, 1, ts);

  if ( sqlite3_step(sql) != SQLITE_ROW ) {
    sqlite3_finalize(sql);
    return 0;
  }

  ret = sqlite3_column_int64(sql, 0);
  sqlite3_finalize(sql);
  return ret;
}

int ug_sqlite_get_gzip_info(sqlite3 *db, uint64_t raw_offset, uint64_t *gz_offset, unsigned char **gz_header) {
  const void *blob;
  int blob_size, ret;
  sqlite3_stmt *sql;

  ret = sqlite3_prepare_v2(db, "SELECT gz_offset, gz_header FROM gz_indices WHERE raw_offset <= ? ORDER BY raw_offset DESC LIMIT 1", -1, &sql, NULL);
  CHECK_RET_INT(ret, db);
  sqlite3_bind_int64(sql, 1, raw_offset);

  if ( sqlite3_step(sql) != SQLITE_ROW ) {
    sqlite3_finalize(sql);
    return 0;
  }

  *gz_offset = sqlite3_column_int64(sql, 0);

  blob = sqlite3_column_blob(sql, 1);
  blob_size = sqlite3_column_bytes(sql, 1);
  *gz_header = malloc(blob_size);
  memcpy(*gz_header, blob, blob_size);

  sqlite3_finalize(sql);
  return 1;
}
