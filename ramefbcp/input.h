#ifndef INPUT_H_INCLUDED
#define INPUT_H_INCLUDED


#ifdef __cplusplus
extern "C" {
#endif


#define INPUT_READBUFSIZE 256

typedef struct _input_ctx
{
    int infd;
    int bufpos; // write pos in buf: [0..INPUT_READBUFSIZE-1]
    char buf[INPUT_READBUFSIZE];
    char eof; // 0 if all ok, 1 if end of file
    char err; // 0 if all ok
} INPUT_CTX;


// input_filedesc can be fileno(stdin) for example
extern INPUT_CTX * input_create(int input_filedesc);

extern void input_close(INPUT_CTX *ctx);

/* Reads a line to dest (with max dest_size including 0 at end)
 * using ctx as the working context.
 * Return values:
 *  1 = new line was written to dest
 *  0 = full line is not yet available
 * -1 = EOF or error
 * NOTE: if the line does not fit in ctx->buf or dest, it will be
 *       truncated and rest is returned as next line!
 */
extern int input_read_line(char *dest, const size_t dest_size, INPUT_CTX *ctx);


#ifdef __cplusplus
}
#endif

#endif // !INPUT_H_INCLUDED
