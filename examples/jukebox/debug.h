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
#include <time.h>
static struct timeval _tv0;
#define DSFYDEBUG(...) {						\
	struct timeval tv;					\
	int s, us;							\
	if(_tv0.tv_sec == 0)						\
		gettimeofday(&_tv0, NULL);				\
	gettimeofday(&tv, NULL);					\
	s = tv.tv_sec; us = tv.tv_usec;					\
	s -= _tv0.tv_sec; us -= _tv0.tv_usec;				\
	if(us < 0) {							\
		us += 1000*1000;					\
		s--;							\
	}								\
	/* fprintf(stderr, "% 2d.%06d %s:%d %s() ", s, us, basename(__FILE__), __LINE__, 	\
			__func__);	*/				\
	fprintf(stderr, "%s:%d %s() ", basename(__FILE__), __LINE__,	\
			__func__);					\
	fprintf(stderr, __VA_ARGS__);					\
}
#endif
#else
#define DSFYDEBUG(...)
#endif

#endif
