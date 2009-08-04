#ifndef LIBOPENSPOTIFY_DEBUG_H
#define LIBOPENSPOTIFY_DEBUG_H

#ifdef DEBUG
#ifdef _WIN32
#define DSFYDEBUG(...) { FILE *fd = fopen("despotify.log","at"); fprintf(fd, "%s:%d %s() ", __FILE__, __LINE__, __FUNCTION__); fprintf(fd, __VA_ARGS__); fclose(fd); }
#else
#define DSFYDEBUG(...) { FILE *fd = fopen("/tmp/despotify.log","at"); fprintf(fd, "%s:%d %s() ", __FILE__, __LINE__, __func__); fprintf(fd, __VA_ARGS__); fclose(fd); }
#endif
#else
#define DSFYDEBUG(...)
#endif

#endif
