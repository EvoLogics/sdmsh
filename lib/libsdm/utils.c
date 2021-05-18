#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include <utils.h>

#define LOGGER_MAX_LINE 1024

static char last_log_line[LOGGER_MAX_LINE];
static int  log_fd = -1;

int logger_init(const char *filename, int limit)
{
    if (log_fd != -1)
        close(log_fd);

    if (!filename)
        return -1;

    log_fd = open(filename,O_CREAT | O_RDWR,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    if (log_fd < 0)
        return -1;

    return logger_init_fd(log_fd, limit);
}

int logger_init_fd(int fd, int limit)
{
    if (limit > 0)
        return 0;
    log_fd = fd;
    return 0;
}

void logger_deinit()
{
    if (log_fd != -1)
        close(log_fd);
    log_fd = -1;
}

int logger_(const char *msg, ...)
{
    va_list ap;
    int rc, fd = STDOUT_FILENO;

    va_start(ap, msg);
    vsnprintf(last_log_line, sizeof(last_log_line), msg, ap);
    va_end(ap);

    if (log_fd != -1)
        fd = log_fd;

    va_start(ap, msg);
    rc = vdprintf(fd, msg, ap);
    va_end(ap);

    return rc;
}

char *logger_last_line()
{
    return last_log_line;
}

void hex_dump(FILE *f, char buf[], unsigned int len)
{                                                        
    unsigned int i;                                                 
    for (i = 0; i < len; i++)                              
        fprintf(f, "%02x ", buf[i]); 
    /*fprintf(f,"%02x ",*((unsigned char *)(buf) + i)); */
}

void hex_dump_short(FILE *f, char buf[], unsigned int len)
{
    unsigned int i;
    if (len <= 16)
    {                                                 
        hex_dump(f, buf, len);                                          
    }
    else
    {                                                           
        fprintf(f, "len = %d, buf: ", len);                         
        hex_dump(f, buf, 8);                                          
        fprintf(f, "... ");                                          
        for (i = 8; i > 0; i--)                                      
            fprintf(f, "%02x ", buf[len - i]);
    }                                                                
}

/* 
 * kill all spaces at the start and the end of a string
 */
char* strchopspaces(char *str)
{
    char *pb = str + strlen(str) - 1;

    if (!str)
        return NULL;
    if (!*str)
        return str;

    while (pb != str && isspace(*pb))
        pb--;
    *(pb + 1) = 0;

    pb = str;
    while (pb && isspace(*pb))
        pb++;
    return pb;
}

/**
 * buffer buf start with string start
 */
char* strstart(char *buf, const char *start)
{
    if (!buf || !start)
        return NULL;

    do {
        if (*start == '\0')
            return buf;
    }while (*start++ == *buf++);

    return NULL;
}

/**
 * buffer buf start with string start
 */
char* strnstart(char *buf, const char *start, int buf_len)
{
    char *c = buf;

    if (!buf || !start)
        return NULL;

    do {
        if (*start == '\0')
            return c;
        if (c - buf == buf_len) 
            return c; // not enough data
    } while (*start++ == *c++);

    return NULL;
}

/**
 * remove blank
 */
void strdelspaces(char *str)
{
    char *sp = str; /* start ptr */
    for (; (*sp = *str); sp++, str++)
        if (isspace(*str))
            sp--;
}

char *strtoncat(char *dest, size_t n, const char *src)
{
    return strncat(dest, src, n - strlen(dest) - 1);
}

/* memmem truncated, needle can be not fully in haystack :) */
#define MEMMEM_HELPER(func) do {                                        \
    for (i = 0; i < haystacklen; i++) {                             \
        if (func(haystack[i]) == func(needle[0]))                   \
        {                                                           \
            p = haystack + i;                                       \
            for (j = i + 1; j < haystacklen && j - i < needlelen; j++) { \
                if (func(haystack[j]) != func(needle[j - i])) {     \
                    p = NULL;                                       \
                    break;                                          \
                }                                                   \
            }                                                       \
            if (p && j - i >= minlen) {                             \
                return p;                                           \
            }                                                       \
        }                                                           \
    }                                                               \
} while (0)
static char *memmem_minlen(char *haystack, size_t haystacklen, char *needle, size_t needlelen, size_t minlen, char iscase_sensitive)
{
    size_t i, j;
    char *p = NULL;

    if (!haystack || !needle)
        return p;

    if (iscase_sensitive) {
        MEMMEM_HELPER();
    } else {
        MEMMEM_HELPER(toupper);
    }

    return NULL;
}

void memleft_shift(void *dest, size_t dest_offset, size_t src_offset, size_t n)
{
    if (dest_offset > src_offset) {
        logger(ERR_LOG,LRED"Error! dest_offset %d < src_offset %d\n", dest_offset, src_offset);
        exit(-1);
    }

    memmove(dest + dest_offset, dest + src_offset, n);

    if (dest_offset + n < src_offset) {
        memset(dest + src_offset, 0, n);
    } else {
        memset(dest + dest_offset + n, 0, src_offset - dest_offset);
    }

    logger(DEBUG_LOG,LBLUE"n = %d, dest_offset %d, src_offset %d\n"DEF, n, dest_offset, src_offset);
}

char *memmemtr(char *haystack, size_t haystacklen, char *needle, size_t needlelen)
{
    return memmem_minlen(haystack, haystacklen, needle, needlelen, 1, 1);
}

char *memmemf(char *haystack, size_t haystacklen, char *needle, size_t needlelen)
{
    return memmem_minlen(haystack, haystacklen, needle, needlelen, needlelen, 1);
}

/* not case sensitive */
char *memmemf_ncase(char *haystack, size_t haystacklen, char *needle, size_t needlelen)
{
    return memmem_minlen(haystack, haystacklen, needle, needlelen, needlelen, 0);
}

int memncat(char *dst, size_t dst_len, char *src, size_t src_len, size_t max_len)
{
    if (dst_len + src_len > max_len)
    {
        logger(ERR_LOG, LRED"data buffer owerflow. len = %d, some data drop\n"DEF
                ,dst_len + src_len);
        src_len = max_len - dst_len;
    }

    memcpy(dst + dst_len, src, src_len);
    return src_len;
}

char* str2ctrlseq(char *src, char *dst, int len)
{
    int i;
    char *d = dst;

    if (src == NULL)
        return NULL;

    for (i = 0; *src && i < len; src++, d++, i++)
    {
        *d= *src;
        switch (*src)
        {
            case '\\': *(++d) = '\\';         break;
            case '\t': *d++ = '\\'; *d = 't'; break;
            case '\n': *d++ = '\\'; *d = 'n'; break;
            case '\r': *d++ = '\\'; *d = 'r'; break;
            default:                          break;
        }
    }
    *d = '\0';
    return dst;
}

char* strexpandctrlseq(char *src)
{
    char *sp, *ep;
    for (sp = ep = src; *ep; sp++, ep++)
    {
        if (*ep != '\\')
            *sp = *ep;
        else
            switch (*(++ep))
            {
                case '\\': *sp = '\\'; break;
                case 't':  *sp = '\t'; break;
                case 'n':  *sp = '\n'; break;
                case 'r':  *sp = '\r'; break;
                default:
                           --ep;
            }
    }
    *sp = '\0';
    return src;
}

#define SSPRINTFMAX 0x1ff
/**
 * snprintf to static allocated buffer
 */
char* ssprintf(char *fmt, ...)
{
    static char buf[SSPRINTFMAX];
    va_list ap;

    va_start(ap, fmt);

    if (vsnprintf(buf, SSPRINTFMAX, fmt, ap) >= SSPRINTFMAX)
        logger(ERR_LOG, LRED"cut string \"%s\"\n"DEF, buf);

    va_end(ap);

    return buf;
}

char *sstrpad(char *src, size_t pad)
{
    static char buf[SSPRINTFMAX];
    size_t len = strlen(src);

    if (pad > sizeof(buf) - 1) {
        pad = sizeof(buf) - 1;
    }
    buf[0] = 0;
    if (pad >= len) {
        strcat(buf, src);
        memset(&buf[len], ' ', pad - len);
        buf[pad] = 0;
    } else {
        strncat(buf, src, pad);
        buf[pad-1] = '{';
    }
    return buf;
}

void log_hexdump(char *buf, ssize_t len)
{
    static char hex[]="0123456789abcdef";

    /* make wider format (32 byte pro line with space colon in middle) with ASCII */
    /* char cache[(CACHE_SIZE * 3 + 1) + (CACHE_SIZE + 1) + 1]; [> 0: xx xx ... xx  ascii <] */
    char cache[(CACHE_SIZE * 2 + CACHE_SIZE / 2 + 1) + (CACHE_SIZE + 1)+ 1] = {'a'}; 
    /* 
     * 00000: xx xx ... xx xx  xx xx .. xx  ascii .. ascii
     *   pc1--^    pc2--^              pc3--^   pc4--^
     */

    char *pc1, *pc2, *pc3, *pc4;
    char *pbuf1, *pbuf2;
    int i, j;
    int tail = len % CACHE_SIZE;

    if (CACHE_SIZE <= len)
        logger_ ("\n");

    pbuf1 = pbuf2 = buf;

    for (j = 0; j < len / CACHE_SIZE; j++)
    {
        pc1 = cache;

        pc2 = cache + (CACHE_SIZE + CACHE_SIZE / 4);
        /* #define LOGGER_DUMP_WITH_SPACE */
#ifdef LOGGER_DUMP_WITH_SPACE
        *pc2++ = ' ';
#  define LOGGER_DUMP_ASCII_SHIFT (CACHE_SIZE * 2 + CACHE_SIZE / 2 + 1)
#else
#  define LOGGER_DUMP_ASCII_SHIFT (CACHE_SIZE * 2 + CACHE_SIZE / 2)
#endif

        pbuf2 += (CACHE_SIZE / 2);
        pc3 = cache + LOGGER_DUMP_ASCII_SHIFT;
        *pc3++ = ' ';

        pc4 = cache + LOGGER_DUMP_ASCII_SHIFT + (CACHE_SIZE / 2) + 1;

        for (i = 0; i < CACHE_SIZE / 4; i++)
        {
            *pc3++ = isprint(*pbuf1) ? *pbuf1 : '.';
            *pc4++ = isprint(*pbuf2) ? *pbuf2 : '.';

            *pc1++ = hex[(*pbuf1 >> 4) & 0x0f]; *pc1++ = hex[*pbuf1++ & 0x0f];
            *pc3++ = isprint(*pbuf1) ? *pbuf1 : '.';
            *pc1++ = hex[(*pbuf1 >> 4) & 0x0f]; *pc1++ = hex[*pbuf1++ & 0x0f]; *pc1++ = ' ';

            *pc2++ = hex[(*pbuf2 >> 4) & 0x0f]; *pc2++ = hex[*pbuf2++ & 0x0f];
            *pc4++ = isprint(*pbuf2) ? *pbuf2 : '.';
            *pc2++ = hex[(*pbuf2 >> 4) & 0x0f]; *pc2++ = hex[*pbuf2++ & 0x0f]; *pc2++ = ' ';
        }
        pbuf1 += (CACHE_SIZE / 2);
        *pc4 = 0;

        logger_ ("%05x: %s\n", (pbuf2 - buf - CACHE_SIZE), cache);
    }

    if (tail)
    {
        int space = 1;
        pc1 = cache;

        pc3 = cache + LOGGER_DUMP_ASCII_SHIFT;
        *pc3++ = ' ';

        for (i = 0; i < CACHE_SIZE; i++)
        {
            if (i >= tail)
            {
                *pc3++ = ' ';
                *pc1++ = ' '; *pc1++ = ' ';
            }
            else
            {
                *pc3++ = isprint(*pbuf1) ? *pbuf1 : '.';
                *pc1++ = hex[(*pbuf1 >> 4) & 0x0f]; *pc1++ = hex[*pbuf1++ & 0x0f];
            }
            if (space ^= 1)
                *pc1++ = ' ';
#ifdef LOGGER_DUMP_WITH_SPACE
            if (i == CACHE_SIZE/2 - 1)
                *pc1++ = ' ';
#endif
        }
        *pc3 = 0;
        logger_ ("%05x: %s\n", (pbuf1 - buf - tail), cache);
    }

}

int array2hexdump(char *dst, size_t dst_len, char *src, size_t src_len)
{
    static char hex[]="0123456789abcdef";
    char *pd = dst;
    char *ps = src;
    unsigned int i;

    if (dst == NULL)
        return src_len * 3;

    if (src_len * 3 > dst_len)
        return -1;

    for (i = 0; i < src_len && i < dst_len; i++) {
        *pd++ = hex[(*ps >> 4) & 0x0f];
        *pd++ = hex[*ps++ & 0x0f];
        *pd++ = ' ';
    }
    *pd = 0;
    return (pd - dst);
}

int hexdump2array(char *dst, size_t dst_len, char *src, size_t src_len)
{
    char *pd = dst;
    char *ps = src;
    unsigned int offset = 0; /* число половинок char */
    char c;

    for (; ps - src < (ssize_t)src_len; ps++) {
        if (*ps >= '0'      && *ps <= '9')
            c = *ps - '0';
        else if (*ps >= 'a' && *ps <= 'f')
            c = *ps - 'a' + 10;
        else if (*ps >= 'A' && *ps <= 'F')
            c = *ps - 'A' + 10;
        else if (isspace(*ps))
            continue;
        else
            return -1;
        if (dst != NULL && (offset+1)/2 <= dst_len) {
            if (offset % 2 ) {
                /* *pd++ = c | *pd; NO-NO! */
                *pd = c | *pd;
                pd++;
            } else {
                *pd = c << 4;
            }
        }
        offset++;
    }

    if (ps - src < (ssize_t)src_len) 
        goto hexdump2array_error;

    if (dst == NULL && dst_len == 0) {
        return (offset+1)/2;
    }

    if (offset/2 < dst_len && dst) {
        return (offset+1)/2;
    }

hexdump2array_error:
    logger(ERR_LOG, "Error! Cannot conver string to hex array,"
            " ps - src = %d, src_len = %d, offset = %d, dst_len = %d, dst = %p\n"
            , ps - src
            , src_len
            , offset
            , dst_len
            , dst);

    return -1;
}

int str2int_array(char *str,int *result, int max_len) 
{
    char *endptr, *startptr;
    int len, i = 0;
    long long x;

    startptr = str;
    len = strlen(str);
    if (max_len < 1)
        return 0;
    do {
        x = strtol(startptr,&endptr,10);
        if (startptr != endptr)
        {
            memcpy(&result[i],&x,sizeof(result[i]));
            i++;
        }
        startptr = endptr + 1;
    } while (x != LONG_MIN && x != LONG_MAX && str + len > startptr && i < max_len);
    return i;
}

int str2float_array(char *str, float *arr, int max_len) 
{
    char *endptr, *startptr;
    int len, i = 0;
    float x;

    startptr = str;
    len = strlen(str);
    if (max_len < 1)
        return 0;
    do {
        errno = 0;
        x = strtof(startptr,&endptr);
        if (startptr != endptr) {
            memcpy(&arr[i],&x,sizeof(arr[i]));
            i++;
        }
        if (errno || endptr == startptr) break;
        startptr = endptr + 1;
    } while (str + len > startptr && i < max_len);
    return i;
}
