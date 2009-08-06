#ifndef ADLER32_H
#define ADLER32_H

unsigned long adler32_update(unsigned long adler, const unsigned char *data, int len);
unsigned long adler32(unsigned char *data, int len);

#endif /* ADLER32_H */
