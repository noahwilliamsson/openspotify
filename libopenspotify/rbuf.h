#ifndef LIBOPENSPOTIFY_RBUF_H
#define LIBOPENSPOTIFY_RBUF_H

struct region {
	size_t len;
	void *data;
};
struct rbuf {
	size_t read_offset;
	size_t write_offset;
	unsigned int n_regions;
	struct region **regions;
};


void *rbuf_new(void);
void rbuf_free(struct rbuf* b);
void rbuf_seek_reader(struct rbuf *b, size_t offset, int whence);
void rbuf_seek_writer(struct rbuf *b, size_t offset, int whence);
size_t rbuf_tell(struct rbuf *b);
void rbuf_write(struct rbuf *b, void *data, size_t len);
size_t rbuf_read(struct rbuf *b, void *dest, size_t len);
size_t rbuf_length(struct rbuf *b);
#endif
