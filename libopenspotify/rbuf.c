/*
 * A buffer implementation that acts like a sparse file
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "rbuf.h"

#define START_SIZE 64*1024
#define CHUNK_SIZE 4096


/*
 * Initialize a buffer
 *
 */
void *rbuf_new(void) {
	struct rbuf* b = malloc(sizeof(struct rbuf));
	assert(b);

	b->read_offset = 0;
	b->write_offset = 0;

	b->n_regions = START_SIZE / CHUNK_SIZE;
	b->regions = (struct region **)calloc(b->n_regions, sizeof(struct region *));

	return b;
}


/*
 * Release resources held by a buffer
 *
 */
void rbuf_free(struct rbuf* b) {
	int n;

	assert(b);
	for(n = 0; n < b->n_regions; n++)
		if(b->regions[n])
			free(b->regions[n]);

	free(b->regions);
	free(b);
}


/*
 * Seeking in the buffer
 *
 */
static void rbuf_seek(struct rbuf *b, size_t offset, int whence, int writer) {

	if(writer) {
		if(whence == SEEK_SET)
			b->write_offset = offset;
		else if(whence == SEEK_CUR)
			b->write_offset += offset;
		else if(whence == SEEK_END)
			b->write_offset = b->n_regions * CHUNK_SIZE - offset;
	}
	else {
		if(whence == SEEK_SET)
			b->read_offset = offset;
		else if(whence == SEEK_CUR)
			b->read_offset += offset;
		else if(whence == SEEK_END)
			b->read_offset = b->n_regions * CHUNK_SIZE - offset;

	}
}


void rbuf_seek_reader(struct rbuf *b, size_t offset, int whence) {

	rbuf_seek(b, offset, whence, 0);
}


void rbuf_seek_writer(struct rbuf *b, size_t offset, int whence) {

	rbuf_seek(b, offset, whence, 1);
}


/*
 * Get the current offset
 *
 */
size_t rbuf_tell(struct rbuf *b) {

	return b->read_offset;
}


/*
 * Write a chunk of data into at the buffer's current position
 *
 */
void rbuf_write(struct rbuf *b, void *data, size_t len) {
	unsigned int n;
	struct region *reg;
	void *ptr;
	size_t remaining;
	size_t reg_offset, nbytes;

	ptr = data;
	remaining = len;
	while(remaining) {
		/* Determine which region to write to */
		n = b->write_offset / CHUNK_SIZE;
		if(n >= b->n_regions) {
			b->regions = realloc(b->regions, sizeof(struct region *) * (n + 1));
			while(b->n_regions <= n) {
				b->regions[b->n_regions++] = NULL;
				assert(b->regions[b->n_regions - 1] == NULL);
			}
			assert(b->regions[n] == NULL);
		}

		if((reg = b->regions[n]) == NULL) {
			b->regions[n] = reg = malloc(sizeof(struct region) + CHUNK_SIZE);
			reg->len = 0;
			reg->data = (unsigned char *)reg + sizeof(struct region);
		}

		/* Figure out where to write */
		reg_offset = b->write_offset % CHUNK_SIZE;

		/* Make sure we don't overflow */
		nbytes = CHUNK_SIZE - reg_offset;
		if(nbytes > remaining)
			nbytes = remaining;

		/* Copy in data */
		memcpy(reg->data + reg_offset, ptr, nbytes);

		/* Update region's data length */
		if(reg->len < reg_offset + nbytes)
			reg->len = reg_offset + nbytes;

		/* Update buffer's position */
		b->write_offset += nbytes;

		/* Continue */
		ptr += nbytes;
		remaining -= nbytes;
	}
}


/*
 * Read data from the buffer's current position
 * into a destination buffer.
 *
 */
size_t rbuf_read(struct rbuf *b, void *dest, size_t len) {
	int n;
	struct region *reg;
	size_t remaining;
	void *ptr;
	size_t reg_offset, nbytes;

	ptr = dest;
	remaining = len;
	while(remaining) {
		/* Determine which region to read from */
		n = b->read_offset / CHUNK_SIZE;
		if(n >= b->n_regions) {
			/* We don't have this many regions */

			break;
		}
		else if((reg = b->regions[n]) == NULL) {
			/* Region not present, can't read more */
			break;
		}

		/* Figure out where to reaad from */
		reg_offset = b->read_offset % CHUNK_SIZE;
		if(reg_offset > reg->len) {
			/* This part of the buffer is undefined */
			break;
		}

		nbytes = reg->len - reg_offset;
		if(nbytes > remaining)
			nbytes = remaining;

		/* Copy out data */
		memcpy(ptr, reg->data + reg_offset, nbytes);

		/* Update buffer's position */
		b->read_offset += nbytes;

		/* Continue */
		ptr += nbytes;
		remaining -= nbytes;
	}

	return len - remaining;
}


/*
 * Return the number of bytes that can be
 * read from the current position
 *
 */
size_t rbuf_length(struct rbuf *b) {
	unsigned int n;
	struct region *reg;
	size_t offset;
	size_t len;
	size_t reg_offset;

	/* Cache offset */
	offset = b->read_offset;

	len = 0;
	do {
		/* Determine which region to read from */
		n = offset / CHUNK_SIZE;
		if(n >= b->n_regions) {
			/* We don't have this many regions */
			break;
		}
		else if((reg = b->regions[n]) == NULL) {
			/* Region not present */
			break;
		}

		/* Figure out start of the buffer */
		reg_offset = offset % CHUNK_SIZE;
		if(reg_offset > reg->len) {
			/* This part of the buffer is undefined */
			break;
		}

		len += reg->len - reg_offset;
		offset += reg->len - reg_offset;
	} while(reg->len == CHUNK_SIZE);

	return len;
}
