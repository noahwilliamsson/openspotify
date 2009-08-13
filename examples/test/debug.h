#ifndef LIBOPENSPOTIFY_DEBUG_H
#define LIBOPENSPOTIFY_DEBUG_H

#ifdef DEBUG
#include <stdio.h>

#ifdef _WIN32
#define DSFYDEBUG(...) {				\
	char *file = __FILE__, *ptr;			\
	for(ptr = file; *ptr; ptr++);			\
	while(ptr != file && *ptr-- != '\\');		\
	if(ptr != file) ptr += 2;			\
	fprintf(stderr, "%s:%d %s() ", ptr, __LINE__,	\
                        __FUNCTION__);			\
	fprintf(stderr, __VA_ARGS__);			\
}
#else
#include <libgen.h>
#define DSFYDEBUG(...) {						\
	fprintf(stderr, "%s:%d %s() ", basename(__FILE__), __LINE__,	\
			__func__);					\
	fprintf(stderr, __VA_ARGS__);					\
}
#endif
#else
#define DSFYDEBUG(...)
#endif

#endif
