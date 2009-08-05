#ifndef LIBOPENSPOTIFY_DEBUG_H
#define LIBOPENSPOTIFY_DEBUG_H

#ifdef DEBUG
#include <stdio.h>

#ifdef _WIN32
#define DSFYDEBUG(...) {                                        \
        FILE *fd;                                               \
        char *file = __FILE__, *ptr;                            \
                                                                \
        for(ptr = file; *ptr; ptr++);                           \
        while(ptr != file && *ptr-- != '\\');                   \
        if(ptr != file) ptr += 2;                               \
        if((fd = fopen("despotify.log", "at")) != NULL) {       \
                fprintf(fd, "%s:%d %s() ", ptr, __LINE__,       \
                        __FUNCTION__);                          \
                fprintf(fd, __VA_ARGS__);                       \
                fclose(fd);                                     \
	}							\
}
#else
#define DSFYDEBUG(...) {                                        \
        FILE *fd;                                               \
        if((fd = fopen("despotify.log", "at")) != NULL) {       \
                fprintf(fd, "%s:%d %s() ", __FILE__, __LINE__,	\
                        __func__);				\
                fprintf(fd, __VA_ARGS__);			\
                fclose(fd);					\
        }							\
}
#endif
#else
#define DSFYDEBUG(...)
#endif

#endif
