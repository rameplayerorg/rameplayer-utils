/* Copyright 2015 rameplayerorg
 * Licensed under GPLv2, which you must read from the included LICENSE file.
 *
 * Info display code for part of the LCD screen (secondary framebuffer).
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

#include "input.h"


// input_filedesc can be fileno(stdin) for example
INPUT_CTX * input_create(int input_filedesc)
{
    INPUT_CTX *ctx = (INPUT_CTX *)calloc(1, sizeof(INPUT_CTX));
    if (ctx == NULL)
    {
        fprintf(stderr, "Can't alloc input context\n");
        return NULL;
    }
    ctx->infd = input_filedesc;
    ctx->bufpos = 0;
    memset(ctx->buf, 0, INPUT_READBUFSIZE);
    ctx->eof = 0;
    ctx->err = 0;
    return ctx;
}

void input_close(INPUT_CTX *ctx)
{
    free(ctx);
}


/* Reads a line to dest (with max dest_size including 0 at end)
 * using ctx as the working context.
 * Return values:
 *  1 = new line was written to dest
 *  0 = full line is not yet available
 * -1 = EOF or error
 * NOTE: if the line does not fit in ctx->buf or dest, it will be
 *       truncated and rest is returned as next line!
 */
int input_read_line(char *dest, const size_t dest_size, INPUT_CTX *ctx)
{
    fd_set rfds;
    struct timeval timeout;
    int a, status;
    int maxbytes;
    ssize_t nread;
    int linelength;

    if (ctx == NULL || ctx->err || dest == NULL)
        return -1;

    if (!ctx->eof)
    {
        int fd = ctx->infd;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        status = select(fd + 1, &rfds, NULL, NULL, &timeout);
        if (status == -1)
        {
            perror("Error in input_read_line select()");
            ctx->err = 1;
            return -1;
        }
        else if (status == 0)
        {
            // not enough data yet
            return 0;
        }

        // read data to input (can be partial line or several lines)
        maxbytes = INPUT_READBUFSIZE - ctx->bufpos - 1;
        nread = read(fd, ctx->buf + ctx->bufpos, maxbytes);
        if (nread == -1)
        {
            perror("Error in input_read_line read()");
            ctx->err |= 2;
            return -1;
        }
        else if (nread == 0)
        {
            ctx->eof = 1; // end of file
        }
        else
        {
            //fprintf(stdout, "nread %d\n", nread); fflush(stdout);
            ctx->bufpos += nread;
            ctx->buf[ctx->bufpos] = 0; // available part is always zero-terminated
        }
    } // !eof

    linelength = -1;
    for (a = 0; a < ctx->bufpos; ++a)
    {
        if (ctx->buf[a] == '\n')
        {
            linelength = a;
            ctx->buf[a] = 0;
            break;
        }
    }

    if (linelength == -1 && ctx->eof)
    {
        // reached eof and no newline at end, return as line
        linelength = ctx->bufpos;
    }

    if (linelength >= 0)
    {
        // ready to return a line
        int destlinelen = (linelength > dest_size - 1) ? dest_size - 1 : linelength;
        memcpy(dest, ctx->buf, destlinelen + 1); // includes pre-written zero at end

        memmove(ctx->buf, ctx->buf + destlinelen + 1, ctx->bufpos + 1 - (destlinelen + 1));
        ctx->bufpos -= destlinelen + 1;

        return 1;
    }

    if (ctx->eof)
        return -1;

    return 0; // no data?
}
