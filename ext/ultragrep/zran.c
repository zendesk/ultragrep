// ex: set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
/* zran.c -- example of zlib/gzip stream indexing and random access
 * Copyright (C) 2005 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
   Version 1.0  29 May 2005  Mark Adler */

/* for gzipped logs we keep two indexes.  One is like this:
 * [timestamp, uncompressed_offset]
 * [timestamp, uncompressed_offset]
 * ...
 *
 * And one is like this:
 * [uncompressed_offset, compressed_offset, gzip_data]
 * [uncompressed_offset, compressed_offset, gzip_data]
 *
 * so we can first get the uncompressed offset and then start extracting in the
 * file from the correct spot.  Mark Adler's comments follow. */

/* Illustrate the use of Z_BLOCK, inflatePrime(), and inflateSetDictionary()
   for random access of a compressed file.  A file containing a zlib or gzip
   stream is provided on the command line.  The compressed stream is decoded in
   its entirety, and an index built with access points about every SPAN bytes
   in the uncompressed output.  The compressed file is left open, and can then
   be read randomly, having to decompress on the average SPAN/2 uncompressed
   bytes before getting to the desired block of data.

   An access point can be created at the start of any deflate block, by saving
   the starting file offset and bit of that block, and the 32K bytes of
   uncompressed data that precede that block.  Also the uncompressed offset of
   that block is saved to provide a referece for locating a desired starting
   point in the uncompressed stream.  build_index() works by decompressing the
   input zlib or gzip stream a block at a time, and at the end of each block
   deciding if enough uncompressed data has gone by to justify the creation of
   a new access point.  If so, that point is saved in a data structure that
   grows as needed to accommodate the points.

   To use the index, an offset in the uncompressed data is provided, for which
   the latest accees point at or preceding that offset is located in the index.
   The input file is positioned to the specified location in the index, and if
   necessary the first few bits of the compressed data is read from the file.
   inflate is initialized with those bits and the 32K of uncompressed data, and
   the decompression then proceeds until the desired offset in the file is
   reached.  Then the decompression continues to read the desired uncompressed
   data from the file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"
#include "ug_index.h"

#define WINSIZE 32768U      /* sliding window size */
#define CHUNK 16384         /* file input buffer size */
// how often (in uncompressed bytes) to add an index
#define INDEX_EVERY_NBYTES 30000000


/*
 * parse the contents of the circular gzip buffer line by line.  when a request
 * spans gzip blocks, we have to leave the request in the buffer and wait for the next block
 *
 * maintains 3 pointers - where we would read next in the buffer, where we're writing next, and the
 * allocated outputbuffer.
 */

struct gz_output_context
{
    char *window;
    int window_len;

    char *start; // point in the window where the data is to be read from

    char *line; // buffer for an output line
    int line_len;
    off_t total_out;
    off_t total_in;
    off_t last_index_offset;

    build_idx_context_t *build_idx_context;
};


void process_circular_buffer(struct gz_output_context *c)
{
    struct ug_index *tmp;
    char *p;

    for(;;) { 
        p = c->start;

        /* skip to newline or end of buffer */
        while ( (*p != '\n') && ((p - c->window) < (c->window_len)) ) 
            p++;

        c->line = realloc(c->line, (p - c->start) + c->line_len + 1);
        memcpy(c->line + c->line_len, c->start, p - c->start);
        c->line_len += (p - c->start);
        c->total_out += (p - c->start);

        if ( p == (c->window + c->window_len) ) {
            if ( c->window_len == WINSIZE ) /* buffer is full, wrap back to top of input buffer to complete the line */
                c->start = c->window;
            else /* out of data in the input buffer but the line didn't terminate, continue to next block to complete the line */
                c->start = p;
            return;
        } else { 
            c->build_idx_context->m->process_line(c->build_idx_context->m, c->line, c->line_len + 1, c->total_out - c->line_len); 
            
            c->line = NULL;
            c->line_len = 0;
            c->start = p + 1;
            c->total_out += 1;
        }
    }
}

int need_gz_index(z_stream *strm, struct gz_output_context *c) 
{
    if ( !((strm->data_type & 128) && !(strm->data_type & 64) ) )
        return 0;
    return c->last_index_offset == 0 || (c->total_out - c->last_index_offset) > INDEX_EVERY_NBYTES;
}


void add_gz_index(z_stream *strm, struct gz_output_context *c, char *window)
{
    unsigned char gz_index_data[WINSIZE];
    off_t compressed_offset;

    compressed_offset = (((uint64_t) strm->data_type & 7) << 56);
    compressed_offset |= (c->total_in & 0x00FFFFFFFFFFFFFF);

    fwrite(&(c->total_out), sizeof(off_t), 1, c->build_idx_context->fgzindex);
    fwrite(&compressed_offset, sizeof(off_t), 1, c->build_idx_context->fgzindex);
    
    if (strm->avail_out) {
        fwrite(window + (WINSIZE - strm->avail_out), strm->avail_out, 1, c->build_idx_context->fgzindex);
    }

    /* copy from beginning -> middle of buffer if needed */
    if (strm->avail_out < WINSIZE)
        fwrite(window, WINSIZE - strm->avail_out, 1, c->build_idx_context->fgzindex);

    c->last_index_offset = c->total_out;
}

/* Make one entire pass through the compressed stream and build an index, with
   access points about every span bytes of uncompressed output -- span is
   chosen to balance the speed of random access against the memory requirements
   of the list, about 32K bytes per access point.  Note that data after the end
   of the first zlib or gzip stream in the file is ignored.  build_index()
   returns the number of access points on success (>= 1), Z_MEM_ERROR for out
   of memory, Z_DATA_ERROR for an error in the input file, or Z_ERRNO for a
   file read error.  On success, *built points to the resulting index. */

int build_gz_index(build_idx_context_t *cxt)
{
    int ret, last_line_size;
    z_stream strm;
    unsigned char input[CHUNK];
    unsigned char window[WINSIZE];
    struct gz_output_context output_cxt;

    bzero(&strm, sizeof(z_stream));
    bzero(&output_cxt, sizeof(struct gz_output_context));

    output_cxt.window = output_cxt.start = window;
    output_cxt.build_idx_context = cxt;
    
    ret = inflateInit2(&strm, 47);      /* automatic zlib or gzip decoding */
    if (ret != Z_OK)
        return ret;

    /* inflate the input, maintain a sliding window, and build an index -- this
       also validates the integrity of the compressed data using the check
       information at the end of the gzip or zlib stream */

    strm.avail_out = 0;
    do {
        /* get some compressed data from input file */
        strm.avail_in = fread(input, 1, CHUNK, cxt->flog);
        if (ferror(cxt->flog)) {
            ret = Z_ERRNO;
            goto build_index_error;
        }
        if (strm.avail_in == 0) {
            ret = Z_DATA_ERROR;
            goto build_index_error;
        }
        strm.next_in = input;

        /* process all of that, or until end of stream */
        do {
            /* reset to top of circular buffer */
            if (strm.avail_out == 0) {
                strm.avail_out = WINSIZE;
                strm.next_out = window;
            }

            /* inflate until out of input, output, or at end of block --
               update the total input and output counters */
            output_cxt.total_in += strm.avail_in;
            ret = inflate(&strm, Z_BLOCK);      /* return at end of block */
            output_cxt.total_in -= strm.avail_in;
            if (ret == Z_NEED_DICT)
                ret = Z_DATA_ERROR;
            if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
                goto build_index_error;
            if (ret == Z_STREAM_END)
                break;

            output_cxt.window_len = WINSIZE - strm.avail_out;

            /* process the uncompressed line data so that the timestamp -> uncompressed offset index gets written */
            process_circular_buffer(&output_cxt);

            /*
             *
             * at the start of a gzip block we need to store the last 32k of data, and a 
             * at the end of a gzip block we reset our context information, so if handle_request
             * decides to add an index somewhere inside this block we can have an index to the gzip block. 
             * 
             * note that we store the bit offset in the high byte of the offset field in the index.
             */
            if ( need_gz_index(&strm, &output_cxt) ) {
                add_gz_index(&strm, &output_cxt, window);
            }
        } while (strm.avail_in != 0);
    } while (ret != Z_STREAM_END);

    /* clean up and return index (release unused entries in list) */
    (void)inflateEnd(&strm);
    return 0;

    /* return error */
  build_index_error:
    (void)inflateEnd(&strm);
    return ret;
}

/* target_offset is the offset in the uncompressed stream we're looking for. */
void fill_gz_info(off_t target_offset, FILE *gz_index, char *dict_data, off_t *compressed_offset)
{
    off_t uncompressed_offset = 0,
          tmp;

    for(;;) { 
        if ( !fread(&uncompressed_offset, sizeof(off_t), 1, gz_index) )
            break;

        if ( uncompressed_offset > target_offset ) {
            return;
        }

        if ( !fread(compressed_offset, sizeof(off_t), 1, gz_index) ) 
            break;

        if ( !fread(dict_data, WINSIZE, 1, gz_index) )
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
int extract(FILE *in, uint64_t time, FILE *offset_index, FILE *gz_index)
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
    ret = inflateInit2(&strm, -15);         /* raw inflate */

    bzero(dict, WINSIZE);

    if (ret != Z_OK)
        return ret;

    if ( index ) {
        uncompressed_offset = ug_get_offset_for_timestamp(offset_index, time);
        fill_gz_info(uncompressed_offset, gz_index, dict, &compressed_offset);

        bits = compressed_offset >> 56;
        compressed_offset = (compressed_offset & 0x00FFFFFFFFFFFFFF) - (bits ? 1 : 0);
    } else {
        compressed_offset = bits = 0;
    }

    ret = fseeko(in, compressed_offset, SEEK_SET);
    if (ret == -1)
        goto extract_ret;
    if (bits) {
        ret = getc(in);
        if (ret == -1) {
            ret = ferror(in) ? Z_ERRNO : Z_DATA_ERROR;
            goto extract_ret;
        }
        (void)inflatePrime(&strm, bits, ret >> (8 - bits));
    }

    if ( compressed_offset > 0 )
        inflateSetDictionary(&strm, dict, WINSIZE);

    for(;;) { 
        strm.avail_out = WINSIZE;
        strm.next_out = output;

        if ( !strm.avail_in ) {
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
    (void)inflateEnd(&strm);
    return ret;
}


