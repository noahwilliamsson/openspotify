#ifndef LIBOPENSPOTIFY_DEBUG_H
#define LIBOPENSPOTIFY_DEBUG_H

#ifdef _DEBUG
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

#include <windows.h>
#define WM2STR(x) ((x) == WM_INITDIALOG? "WM_INITDIALOG":		\
	(x) == WM_CREATE? "WM_CREATE":	\
	(x) == WM_DESTROY? "WM_DESTROY":	\
	(x) == WM_QUIT? "WM_QUIT":		\
	(x) == WM_PAINT? "WM_PAINT":		\
	(x) == WM_SIZE? "WM_SIZE":		\
	(x) == WM_NOTIFY? "WM_NOTIFY":	\
	(x) == WM_TIMER? "WM_TIMER":		\
	(x) == WM_COMMAND? "WM_COMMAND":	\
	(x)== WM_USER? "WM_USER":			\
	"unknown WM msg")

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
