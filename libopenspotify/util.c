/*
 * $Id: util.c 399 2009-07-29 11:50:46Z noah-w $
 *
 * Various support routines
 *
 */

#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>
#endif
#include <ctype.h>
#include <errno.h>

#include <zlib.h>

#include "buf.h"
#include "debug.h"
#include "util.h"

unsigned char *hex_ascii_to_bytes (const char *ascii, unsigned char *bytes, int len)
{
	int i;
	unsigned int value;

	for (i = 0; i < len; i++) {
		if (sscanf (ascii + 2 * i, "%02x", &value) != 1)
			return NULL;

		bytes[i] = value & 0xff;
	}

	return bytes;
}

char *hex_bytes_to_ascii (const unsigned char *bytes, char *ascii, int len)
{
	int i;

	for (i = 0; i < len; i++)
		sprintf (ascii + 2 * i, "%02x", bytes[i]);

	return ascii;
}

void hexdump8x32 (char *prefix, void *data, int len)
{
	fhexdump8x32 (stderr, prefix, data, len);
}

void fhexdump8x32 (FILE * file, char *prefix, void *data, int len)
{
	unsigned char *ptr = (unsigned char *) data;
	int i, j;

	fprintf (file, "%s:%s", prefix,
		 len >= 16 ? "\n" : strlen (prefix) >= 8 ? "\t" : "\t\t");
	for (i = 0; i < len; i++) {
		if (i % 32 == 0)
			printf ("\t");
		fprintf (file, "%02x", ptr[i]);
		if (i % 32 == 31) {
			fprintf (file, " [");
			for (j = i - 31; j <= i; j++)
				fprintf (file, "%c",
					 isprint (ptr[j]) ? ptr[j] : '?');
			fprintf (file, "]\n");
		}
		else if (i % 8 == 7)
			fprintf (file, " ");
	}
	if (i % 32) {
		for (j = i; j % 32; j++)
			fprintf (file, "  %s", j % 8 == 7 ? " " : "");
		fprintf (file, "%s[", j % 8 == 7 ? " " : "");
		for (j = i - i % 32; j < i; j++)
			fprintf (file, "%c", isprint (ptr[j]) ? ptr[j] : '?');
		fprintf (file, "]\n");
	}

	if (!len)
		fprintf (file, "<zero length input>\n");
}

void logdata (char *prefix, int id, void *data, int datalen)
{
	char filename[100];
	FILE *fd;

#ifdef _WIN32
	sprintf (filename, "spotify.%s.%d",  prefix, id);
#else
	sprintf (filename, "spotify.%d.%s.%d", getpid(), prefix, id);
#endif
	if ((fd = fopen (filename, "wb")) != NULL) {
		fwrite (data, 1, datalen, fd);
		fclose (fd);
	}

	DSFYDEBUG ("  -- Saving 0x%04x (%d) bytes file '%s'\n", datalen,
		   datalen, filename);
}


ssize_t block_read (int fd, void *buf, size_t nbyte)
{
	unsigned int idx;
	ssize_t n;
	fd_set rfds;
	struct timeval tv;
	int ret;

	idx = 0;
	while (idx < nbyte) {
		if ((n = recv (fd, (char *)buf + idx, nbyte - idx, 0)) <= 0) {
#ifdef _WIN32
			if(n == -1 && WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
#endif
				FD_ZERO(&rfds);
				FD_SET(fd, &rfds);
				tv.tv_sec = 2;
				tv.tv_usec = 0;
				ret = select(fd + 1, &rfds, NULL, NULL, &tv);
				if(!FD_ISSET(fd, &rfds))
					return -1;

				continue;
			}

			return n;
		}
		idx += n;
	}
	return idx;
}


ssize_t block_write (int fd, const void *buf, size_t nbyte)
{
	unsigned int idx;
	ssize_t n;
	fd_set wfds;

	idx = 0;
	while (idx < nbyte) {
		if ((n = send (fd, (char *)buf + idx, nbyte - idx, 0)) <= 0) {
#ifdef _WIN32
			if(n == -1 && WSAGetLastError() == WSAEWOULDBLOCK) {
#else
			if(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
#endif
				FD_ZERO(&wfds);
				FD_SET(fd, &wfds);
				if(select(fd + 1, NULL, &wfds, NULL, NULL) < 0)
					return -1;

				continue;
			}

			return n;
		}
		idx += n;
	}
	return idx;
}


#ifdef __APPLE__
#include <mach/mach_time.h>
#include <sys/types.h>
#include <sys/time.h>
#endif

int get_millisecs(void) {
#ifdef _WIN32
	/* FIXME: Affected by timezone and DST */
	return GetTickCount();
#elif __linux__
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#elif __APPLE__
	static mach_timebase_info_data_t mtid;
	static uint64_t first_mat;
	uint64_t elapsed_ns;
	
	if(!mtid.denom) {
		mach_timebase_info(&mtid);
		first_mat = mach_absolute_time();
	}
	
	elapsed_ns = (mach_absolute_time() - first_mat) * mtid.numer / mtid.denom;
	return elapsed_ns / 1000000;
#else
	static struct timeval first_tv;
	struct timeval tv;

	if(first_tv.tv_sec == 0)
		gettimeofday(&first_tv, NULL);

	gettimeofday(&tv, NULL);
	tv.tv_sec -= first_tv.tv_sec;
	if(tv.tv_usec < first_tv.tv_usec) {
		tv.tv_sec--;
		tv.tv_usec += 1000000;
	}
	tv.tv_usec -= first_tv.tv_usec;
	return tv.tv_sec * 1000 + tv.tv_usec/1000;
#endif
}


struct buf* despotify_inflate(unsigned char* data, int len) {
	int done, offset, rc;
	struct buf *b;
	struct z_stream_s z;

	if(!len)
		return NULL;

	memset(&z, 0, sizeof z);

	rc = inflateInit2(&z, -MAX_WBITS);
	if (rc != Z_OK) {
		DSFYDEBUG("error: inflateInit() returned %d\n", rc);
		return NULL;
	}

	z.next_in = data;
	z.avail_in = len;

	b = buf_new();
	buf_extend(b, 4096);

	offset = 0;
	done = 0;
	while(!done) {
		z.avail_out = b->size - offset;
		z.next_out = b->ptr + offset;

		rc = inflate(&z, Z_NO_FLUSH);
		switch (rc) {
		case Z_OK:
	                /* inflated fine */
	                if (z.avail_out == 0) {
				/* zlib needs more output buffer */
				offset = b->size;
				buf_extend(b, b->size * 2);
	                }
                break;

		case Z_STREAM_END:
	                /* end of input */
	                done = 1;
                break;

		default:
	                /* error */
	                DSFYDEBUG("error: inflate() returned %d\n", rc);
	                done = 1;
	                buf_free(b);
	                b = NULL;
	                break;
		}
	}

	if (b) {
		b->len = b->size - z.avail_out;
		buf_append_u8(b, 0); /* null terminate string */
	}

	inflateEnd(&z);

	return b;
}
