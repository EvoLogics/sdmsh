#ifndef __LOGGER_H
#define __LOGGER_H

/* NOTE: basename version of uclibc in libgen is dangerous, it modifies original string */
#include <stdarg.h>
#include <stddef.h> /* offsetof() */
#include <stdio.h>
#include <libgen.h> /* basename() */
#include <unistd.h> /* ssize_t */

#define CACHE_SIZE (16*2)

#define FATAL_LOG            0x0001
#define ERR_LOG              0x0002
#define WARN_LOG             0x0004
#define INFO_LOG             0x0008
#define NOTE_LOG             0x0010
#define DEBUG_LOG            0x0020
#define TRACE_LOG            0x0040
#define DATA_LOG             0x0080
#define LOCATION_LOG         0x0100
#define ASYNC_LOG            0x0200

#ifndef NO_COLORS

#define DEFCOLOR "\e[0m"
#define DEF      DEFCOLOR
#define RED      "\e[0;31m"
#define LRED     "\e[1;31m"
#define LGREEN   "\e[1;32m"
#define GREEN    "\e[0;32m"
#define DYELLOW  "\e[0;33m"
#define YELLOW   "\e[1;33m"
#define BLUE     "\e[1;34m"
#define DMAGENTA "\e[0;35m"
#define MAGENTA  "\e[1;35m"
#define LBLUE    "\e[1;36m"
#define CYAN     "\e[0;36m"
#define WHITE    "\e[1;37m"
#define GRAY     "\e[0;38m"

#else

#define DEFCOLOR
#define RED
#define LRED
#define LGREEN
#define GREEN
#define YELLOW
#define BLUE
#define DMAGENTA
#define MAGENTA
#define LBLUE
#define CYAN
#define WHITE
#define GRAY

#endif

#include <ctype.h>
#include <errno.h>
#include <string.h> /* for strerror */
#include <sys/queue.h>

#define container_of(ptr, type, member) (type*)((char*)(ptr) - __builtin_offsetof(type, member))

#ifndef STAILQ_LAST
#define STAILQ_LAST(head, type, field)                  \
        (STAILQ_EMPTY((head)) ? NULL :                  \
                 container_of((head)->stqh_last, struct type, field.stqe_next))

#endif

#define LOGFILE stdout

# define logger(level,fmt, ...)                                         \
    do {                                                                \
        if (level & log_level) {                                        \
            if (LOCATION_LOG & log_level)                               \
                logger_("%0s:%0d:(%s) ",basename(__FILE__),__LINE__,__func__); \
            logger_(fmt, ## __VA_ARGS__);                               \
        }                                                               \
    }while(0)

/* Line to log */
#define L2L(x)       logger(x,"\n");
#define LINE2LOG     logger(DEBUG_LOG,"\n");

/*append logger*/
# define rawlog(level,fmt, ...)                 \
    do {                                        \
        if (level & log_level) {                \
            logger_(fmt, ## __VA_ARGS__);       \
        }                                       \
    }while(0)


# define DUMP2LOG(level, buf, len)              \
    do{                                         \
        if (level & log_level) {                \
            logger(level, "[%d] ", len);        \
            log_hexdump(buf, len);              \
        }                                       \
    } while (0)


# define DUMP(level, buf, len)                                      \
    do{                                                             \
        typeof(len) __i;                                            \
        if (level & log_level) {                                    \
            rawlog(level, "[%d]{", len);                            \
            for (__i = 0; __i < len; __i++)                         \
                logger_("%02x ",*((unsigned char *)(buf) + __i));   \
            logger_("}");                                           \
        }                                                           \
    }while(0)

# define DUMPSTRING(level, buf, len)                \
    do{                                             \
        typeof(len) __i; char __c;                  \
        if (level & log_level) {                    \
            logger(level, "str[%03d]=\"", len);     \
            for (__i = 0; __i < len; __i++)         \
            {                                       \
                __c = *((char *)(buf) + __i);       \
                logger_("%c",isalnum(__c)?__c:'_'); \
            }                                       \
            logger_("\"\n");                        \
        }                                           \
    }while(0)

# define DUMP_FD2LOG(level, fdset, maxfd)           \
    do{                                             \
        int __i;                                    \
        if (level & log_level) {                    \
            logger(level,"fdset dump: ");           \
            for (__i = 0; __i < maxfd + 1; __i++)   \
                if(FD_ISSET(__i, fdset))            \
                    logger_("%02d ",__i);           \
            logger_("\n");                          \
        }                                           \
    }while(0)

# define DUMP_SHORT(level, color, buf, len)                             \
    do{                                                                 \
        int __i;                                                        \
        if (level & log_level) {                                        \
            rawlog(level, color);                                       \
            if (len <= 24) {                                            \
                DUMP(level, buf, len);                                  \
            }else{                                                      \
                logger_("[%d]{", len);                                  \
                for (__i = 0; __i < 8; __i++)                           \
                    logger_("%02x ",*((unsigned char *)(buf) + __i));   \
                logger_("... ");                                        \
                for (__i = 8; __i > 0; __i--)                           \
                    logger_("%02x ",*((unsigned char *)(buf) + len - __i)); \
                rawlog(level, "}");                                     \
            }                                                           \
            rawlog(level, "" DEFCOLOR);                                 \
        }                                                               \
    }while(0)

# define DUMP_SHORT_STRING(level, color, buf, len)          \
    do{                                                     \
        int m__i;                                           \
        char m__c;                                          \
        if (level & log_level) {                            \
            rawlog(level, color);                           \
            if (len <= 32) {                                \
                rawlog(level, "\"");                        \
                for (m__i = 0; m__i < (len); m__i++) {      \
                    m__c = *((char *)(buf) + m__i);         \
                    logger_("%c",isprint(m__c)?m__c:'.');   \
                }                                           \
                rawlog(level, "\"");                        \
            } else {                                        \
                rawlog(level, "\"");                        \
                for (m__i = 0; m__i < 16; m__i++) {         \
                    m__c = *((char *)(buf) + m__i);         \
                    logger_("%c",isprint(m__c)?m__c:'.');   \
                }                                           \
                logger_("... ");                            \
                for (m__i = 16; m__i > 0; m__i--) {         \
                    m__c = *((char *)(buf) + len - m__i);   \
                    logger_("%c",isprint(m__c)?m__c:'.');   \
                }                                           \
                rawlog(level, "\"");                        \
            }                                               \
            rawlog(level, "" DEFCOLOR);                     \
        }                                                   \
    }while(0)

#define DUMP_AND_STRING(level, msg, buf, len) { \
        rawlog (level, msg);                    \
        DUMP(level, buf, len);                  \
        rawlog (level, "   ");                  \
        DUMPSTRING(level, buf, len);            \
        rawlog (level, "\n");                   \
    } while (0)

#define PANICLOG                                                        \
    do {                                                                \
        if (ERR_LOG & log_level) {                                      \
            logger_("%0s:%0d:(%s) ",__FILE__,__LINE__,__func__);        \
            logger_(LRED"PANIC[%ld]: %s\n"DEF, errno, estrerror(errno)); \
        }                                                               \
        exit(1);                                                        \
    } while(0)

extern unsigned long log_level;

int  logger_init(const char *filename,int limit);
int  logger_init_fd(int fd, int limit);
void logger_deinit();
int  logger_(const char *msg, ...);
char *logger_last_line();

void hex_dump(FILE *f, char buf[], unsigned int len);
void hex_dump_short(FILE *f, char buf[], unsigned int len);

#define chop_spaces strchopspaces
char* strchopspaces(char *str);
void  strdelspaces(char *str);
char* strstart(char *buf, const char *start);
char* strnstart(char *buf, const char *start, int buf_len);
char* str2ctrlseq(char *src, char *dst, int len);
char* strexpandctrlseq(char *src);

char *strtoncat(char *dest, size_t n, const char *src);

void memleft_shift(void *dest, size_t dest_offset, size_t src_offset, size_t n);
int   memncat(char *dst, size_t dst_len, char *src, size_t src_len, size_t max_len);
char *memmemtr(char *haystack, size_t haystacklen, char *needle, size_t needlelen);
char *memmemf(char *haystack, size_t haystacklen, char *needle, size_t needlelen);
char *memmemf_ncase(char *haystack, size_t haystacklen, char *needle, size_t needlelen);
char* ssprintf(char *fmt, ...);

void log_hexdump(char *buf, ssize_t len);
int array2hexdump(char *dst, size_t dst_len, char *src, size_t src_len);
int hexdump2array(char *dst, size_t dst_len, char *src, size_t src_len);

int str2float_array(char *str, float *arr, int max_len);
int str2int_array(char *str,int *result, int max_len);

#define CATCH_ALL_SIGNAL(handler)                   \
    do {                                            \
        int __i;                                    \
        for (__i = 0; __i < 32; __i++)              \
            switch (__i)                            \
            {                                       \
            case 9:  /* гы... */                    \
            case 28: /* изменение размеров окна*/   \
                break;                              \
            default:                                \
                signal(__i, handler);               \
            }                                       \
    } while (0)
#endif /*__LOGGER_H*/
