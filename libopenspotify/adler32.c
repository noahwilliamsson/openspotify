/* Adler-32 checksum, see http://www.ietf.org/rfc/rfc1950.txt. */

#include <string.h>

#include "adler32.h"

#define BASE 65521 /* largest prime smaller than 65536 */

unsigned long adler32_update(unsigned long adler, const unsigned char *data, int len){
	unsigned long s1 = adler & 0xffff;
	unsigned long s2 = (adler >> 16) & 0xffff;
	int n;

	for(n = 0; n < len; n++){
		s1 = (s1 + data[n]) % BASE;
		s2 = (s2 + s1)      % BASE;
	}

	return (s2 << 16) + s1;
}

unsigned long adler32(unsigned char *data, int len){
	return adler32_update(1L, data, len);
}
