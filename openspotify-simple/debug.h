#ifndef LIBOPENSPOTIFY_DEBUG_H
#define LIBOPENSPOTIFY_DEBUG_H

#ifdef DEBUG
#ifdef _WIN32
#define DSFYDEBUG(...) {                                        \
        FILE *fd;                                               \
        char *file = __FILE__, *ptr;                            \
                                                                \
        for(ptr = file; *ptr; ptr++);                           \
        while(ptr != file && *ptr-- != '\\');                   \
        if(ptr != file) ptr += 2;                               \
        if((fd = fopen("despotify.log", "at")) != NULL) {       \
                fprintf(fd, "[HANDLE 0x08%x] %s:%d %s() ", (unsigned int)GetCurrentThread(), ptr, __LINE__,       \
                        __FUNCTION__);                          \
                fprintf(fd, __VA_ARGS__);                       \
                fclose(fd);                                     \
	}							\
       	fprintf(stderr, "%s:%d %s() ", ptr, __LINE__,		\
                        __FUNCTION__);				\
	fprintf(stderr, __VA_ARGS__);				\
}
#else
#define DSFYDEBUG(...) {                                        \
        FILE *fd;                                               \
        if((fd = fopen("despotify.log", "at")) != NULL) {       \
                fprintf(fd, "[THR 0x%08x] %s:%d %s() ", (unsigned int)pthread_self(), __FILE__, __LINE__,	\
                        __func__);				\
                fprintf(fd, __VA_ARGS__);			\
                fclose(fd);					\
        }							\
       	fprintf(stderr, "%s:%d %s() ", __FILE__, __LINE__,	\
                        __func__);				\
	fprintf(stderr, __VA_ARGS__);				\
}
#endif
#else
#define DSFYDEBUG(...)
#endif

#endif
