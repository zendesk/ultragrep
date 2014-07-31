// ex: set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "ug_index.h"
#include "ug_gzip.h"
#include "zlib.h"

/* target_offset is the offset in the uncompressed stream we're looking for. */
void fill_gz_info(off_t target_offset, FILE * gz_index, unsigned char *dict_data, off_t * compressed_offset)
{
    off_t uncompressed_offset = 0, tmp;

    for (;;) {
        if (!fread(&uncompressed_offset, sizeof(off_t), 1, gz_index))
            break;

        if (uncompressed_offset > target_offset) {
            return;
        }

        if (!fread(compressed_offset, sizeof(off_t), 1, gz_index))
            break;

        if (!fread(dict_data, WINSIZE, 1, gz_index))
            break;
    }
    return;
}

/* Use the index to read len bytes from offset into buf, return bytes read or
   negative for error (Z_DATA_ERROR or Z_MEM_ERROR).  If data is requested past
   the end of the uncompressed data, then extract() will return a value less
   than len, indicating how much as actually read into buf.  This function
   should not return a data error unless the file was modified since the index
   was generated.  extract() may also return Z_ERRNO if there is an error on
   reading or seeking the input file. */
int ug_gzip_cat(FILE * in, uint64_t time, FILE * offset_index, FILE * gz_index)
{
    int ret, skip, bits;
    off_t uncompressed_offset, compressed_offset;
    z_stream strm;
    unsigned char input[CHUNK];
    unsigned char output[WINSIZE], dict[WINSIZE];

    /* initialize file and inflate state to start there */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;


    bzero(dict, WINSIZE);

    if (gz_index && offset_index) {
        uncompressed_offset = ug_get_offset_for_timestamp(offset_index, time);
        fill_gz_info(uncompressed_offset, gz_index, dict, &compressed_offset);

        bits = compressed_offset >> 56;
        compressed_offset = (compressed_offset & 0x00FFFFFFFFFFFFFF) - (bits ? 1 : 0);

        ret = inflateInit2(&strm, -15);     /* raw inflate */
        if (ret != Z_OK)
            return ret;

        ret = fseeko(in, compressed_offset, SEEK_SET);

        if (ret != Z_OK)
            return ret;
    } else {
        compressed_offset = bits = 0;
        strm.avail_in = fread(input, 1, CHUNK, in);
        strm.next_in = input;
 
        ret = inflateInit2(&strm, 47);
    }


    if (ret == -1)
        goto extract_ret;
    if (bits) {
        ret = getc(in);
        if (ret == -1) {
            ret = ferror(in) ? Z_ERRNO : Z_DATA_ERROR;
            goto extract_ret;
        }
        (void) inflatePrime(&strm, bits, ret >> (8 - bits));
    }

    if (compressed_offset > 0)
        inflateSetDictionary(&strm, dict, WINSIZE);

    for (;;) {
        strm.avail_out = WINSIZE;
        strm.next_out = output;

        if (!strm.avail_in) {
            strm.avail_in = fread(input, 1, CHUNK, in);
            strm.next_in = input;
        }

        if (ferror(in)) {
            ret = Z_ERRNO;
            goto extract_ret;
        }

        if (strm.avail_in == 0) {
            ret = Z_DATA_ERROR;
            goto extract_ret;
        }

        ret = inflate(&strm, Z_NO_FLUSH);       /* normal inflate */

        if (ret == Z_NEED_DICT)
            ret = Z_DATA_ERROR;
        if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
            goto extract_ret;

        fwrite(output, WINSIZE - strm.avail_out, 1, stdout);

        /* if reach end of stream, then don't keep trying to get more */
        if (ret == Z_STREAM_END)
            break;
    }

    /* clean up and return bytes read or error */
  extract_ret:
    (void) inflateEnd(&strm);
    return ret;
}
/* 
 * ug_cat -- given a log file and (possibly) a file + (timestamp -> offset) index, cat the file starting 
 *           from about that timestamp 
 */

#define USAGE "Usage: ug_cat file timestamp\n"

int main(int argc, char **argv)
{
    int nread;
    FILE *log;
    FILE *index;
    char *log_fname, *index_fname, buf[4096];

    if (argc < 3) {
        fprintf(stderr, USAGE);
        exit(1);
    }

    log_fname = argv[1];

    log = fopen(log_fname, "r");
    if (!log) {
        perror("Couldn't open log file");
        exit(1);
    }

    index_fname = ug_get_index_fname(log_fname, "idx");

    index = fopen(index_fname, "r");
    if (strcmp(log_fname + (strlen(log_fname) - 3), ".gz") == 0) {
        char *gzidx_fname;
        FILE *gzidx;

        if (index) {
            gzidx_fname = ug_get_index_fname(log_fname, "gzidx");
            gzidx = fopen(gzidx_fname, "r");
            if (!gzidx) {
                perror("error opening gzidx component");
                exit(1);
            }
            ug_gzip_cat(log, atol(argv[2]), index, gzidx);

        } else {
            ug_gzip_cat(log, atol(argv[2]), NULL, NULL);

        }
    } else {
        if (index)
            fseeko(log, ug_get_offset_for_timestamp(index, atol(argv[2])), SEEK_SET);

        while ((nread = fread(buf, 1, 4096, log)))
            fwrite(buf, 1, nread, stdout);
    }
}
