#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <spotify/api.h>

#include "buf.h"
#include "commands.h"
#include "debug.h"
#include "image.h"
#include "hashtable.h"
#include "request.h"
#include "sp_opaque.h"
#include "util.h"

#include <jconfig.h>
#include <jerror.h>
#include <jpeglib.h>


static int osfy_image_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);
static int ofsy_image_update_from_header(sp_image *image);
struct buf *ofsy_image_decompress(sp_image *image, int *pitch);


sp_image *osfy_image_create(sp_session *session, const byte image_id[20]) {
	sp_image *image;

	image = (sp_image *)hashtable_find(session->hashtable_images, image_id);
	if(image) {
		{
			char buf[41];
			hex_bytes_to_ascii(image->id, buf, 20);
			DSFYDEBUG("Returning existing image '%s'\n", buf);
		}
		
		return image;
	}

	image = malloc(sizeof(sp_image));

	memcpy(image->id, image_id, sizeof(image->id));
	image->format = -1;
	image->width = -1;
	image->height = -1;
	image->data = NULL;
	image->raw = NULL;

	image->error = SP_ERROR_RESOURCE_NOT_LOADED;

	image->callback = NULL;
	image->userdata = NULL;

	image->is_loaded = 0;
	image->ref_count = 0;

	image->hashtable = session->hashtable_images;
	hashtable_insert(image->hashtable, image->id, image);

	return image;
}


SP_LIBEXPORT(sp_image *) sp_image_create(sp_session *session, const byte image_id[20]) {
	sp_image *image;
	void **container;
	struct image_ctx *image_ctx;
	

	image = osfy_image_create(session, image_id);
	sp_image_add_ref(image);

	if(sp_image_is_loaded(image))
		return image;


	/* Prevent the image from being loaded twice */
	if(image->error == SP_ERROR_IS_LOADING)
		return image;
	
	image->error = SP_ERROR_IS_LOADING;
	
	
	image_ctx = malloc(sizeof(struct image_ctx));
	image_ctx->session = session;
	image_ctx->req = NULL;
	image_ctx->image = image;

        container = (void **)malloc(sizeof(void *));
        *container = image_ctx;

	{
		char buf[41];
		hex_bytes_to_ascii(image->id, buf, 20);
		DSFYDEBUG("Requesting download of image '%s'\n", buf);
	}

        request_post(session, REQ_TYPE_IMAGE, container);

	return image;
}


SP_LIBEXPORT(void) sp_image_add_load_callback(sp_image *image, image_loaded_cb *callback, void *userdata) {
	/* FIXME: Support multiple callbacks */
	image->callback = callback;
	image->userdata = userdata;

	
	/* FIXME: Check with libspotify */
	if(image->is_loaded)
		callback(image, userdata);
}


SP_LIBEXPORT(void) sp_image_remove_load_callback(sp_image *image, image_loaded_cb *callback, void *userdata) {
	/* FIXME: Support multiple callbacks */
	image->callback = NULL;
	image->userdata = NULL;
}


SP_LIBEXPORT(bool) sp_image_is_loaded(sp_image *image) {

	return image->is_loaded;
}


SP_LIBEXPORT(sp_error) sp_image_error(sp_image *image) {

	return image->error;
}


SP_LIBEXPORT(int) sp_image_width(sp_image *image) {

	return image->width;
}


SP_LIBEXPORT(int) sp_image_height(sp_image *image) {

	return image->height;
}


SP_LIBEXPORT(sp_imageformat) sp_image_format(sp_image *image) {

	return image->format;
}


SP_LIBEXPORT(void *) sp_image_lock_pixels(sp_image *image, int *pitch) {
	struct buf *raw;

	raw = ofsy_image_decompress(image, pitch);

	if(image->raw)
		buf_free(image->raw);

	image->raw = raw;

	return raw->ptr;
}


SP_LIBEXPORT(void) sp_image_unlock_pixels(sp_image *image) {
	if(image->raw)
		buf_free(image->raw);

	image->raw = NULL;
}


SP_LIBEXPORT(const byte *) sp_image_image_id(sp_image *image) {

	return image->id;
}


SP_LIBEXPORT(void) sp_image_add_ref(sp_image *image) {

	image->ref_count++;
}


SP_LIBEXPORT(void) sp_image_release(sp_image *image) {

	assert(image->ref_count);
	if(--image->ref_count)
		return;

	hashtable_remove(image->hashtable, image->id);

	{
		char buf[41];
		hex_bytes_to_ascii(image->id, buf, 20);
		DSFYDEBUG("Freeing image '%s'\n", buf);
	}
	
	if(image->data)
		buf_free(image->data);

	if(image->raw)
		buf_free(image->raw);

	free(image);
}


int osfy_image_process_request(sp_session *session, struct request *req) {
	struct image_ctx *image_ctx = *(struct image_ctx **)req->input;

	req->state = REQ_STATE_RUNNING;
	image_ctx->req = req;

	{
		char buf[41];
		hex_bytes_to_ascii(image_ctx->image->id, buf, 20);
		DSFYDEBUG("Sending request for image '%s'\n", buf);
	}

	assert(image_ctx->image->data == NULL);
	image_ctx->image->data = buf_new();
	
	req->next_timeout = get_millisecs() + IMAGE_RETRY_TIMEOUT * 1000;
	
	return cmd_request_image(session, image_ctx->image->id, osfy_image_callback, image_ctx);
}


static int osfy_image_callback(CHANNEL *ch, unsigned char *payload, unsigned short len) {
	struct image_ctx *image_ctx = (struct image_ctx *)ch->private;
	
	switch(ch->state) {
		case CHANNEL_DATA:
			buf_append_data(image_ctx->image->data, payload, len);
			break;
			
		case CHANNEL_ERROR:
			DSFYDEBUG("Got a channel ERROR, retrying within %d seconds\n", IMAGE_RETRY_TIMEOUT);
			buf_free(image_ctx->image->data);
			image_ctx->image->data = NULL;
			
			/* The request processor will retry this round */
			break;
			
		case CHANNEL_END:
			/* FIXME: Decode image ->data */
			ofsy_image_update_from_header(image_ctx->image);
			image_ctx->image->is_loaded = 1;
			image_ctx->image->error = SP_ERROR_OK;

			request_set_result(image_ctx->session, image_ctx->req, SP_ERROR_OK, image_ctx->image);

			free(image_ctx);
			break;
			
		default:
			break;
	}
	
	return 0;
}


/* jpeglib parser */
#define JPEGBUFSIZE 16*1024
struct jpegsource {
	struct jpeg_source_mgr pub;
	JOCTET *jpegbuf;
	unsigned char *buf;
	int bufpos;
	int buflen;
};


/* jpeglib callbacks */
static void jpeglib_source_init(j_decompress_ptr cinfo) {
	DSFYDEBUG("Initializing source\n");
}


static int jpeglib_source_fill(j_decompress_ptr cinfo) {
	struct jpegsource *src = (struct jpegsource *)cinfo->src;
	int num_bytes_to_copy;

	src->pub.next_input_byte = src->jpegbuf;
	
	num_bytes_to_copy = src->buflen - src->bufpos;
	if(num_bytes_to_copy > JPEGBUFSIZE)
		num_bytes_to_copy = JPEGBUFSIZE;

	DSFYDEBUG("%d bytes copied (len: %d, pos: %d)\n", num_bytes_to_copy, src->buflen, src->bufpos);
	if(num_bytes_to_copy) {
		memcpy(src->jpegbuf, src->buf + src->bufpos, num_bytes_to_copy);
		src->bufpos += num_bytes_to_copy;
		src->pub.bytes_in_buffer = num_bytes_to_copy;	
	}
	else {
		src->jpegbuf[0] = 0xFF;
		src->jpegbuf[1] = JPEG_EOI;
		src->pub.bytes_in_buffer = 2;
	}

	return 1;
}


static void jpeglib_source_slip(j_decompress_ptr cinfo, long num_bytes) {
	struct jpegsource *src = (struct jpegsource *)cinfo->src;

	DSFYDEBUG("Skipping %ld bytes\n", num_bytes);
	if(!num_bytes)
		return;

	while(num_bytes > src->pub.bytes_in_buffer) {
		num_bytes -= src->pub.bytes_in_buffer;
		src->pub.fill_input_buffer(cinfo);
	}

	src->pub.next_input_byte += num_bytes;
	src->pub.bytes_in_buffer -= num_bytes;
}


static void jpeglib_source_destroy(j_decompress_ptr cinfo) {
	struct jpegsource *src = (struct jpegsource *)cinfo->src;

	printf("jpeglib_source_destroy()\n");
	free(src->jpegbuf);
}


/* jpeglib custom input source */
static void osfy_jpeglib_source(j_decompress_ptr cinfo, unsigned char *buf, int buflen) {
	struct jpegsource *src;

	src = (struct jpegsource *)
		(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(struct jpegsource));

	src->buf = buf;
	src->buflen = buflen;
	src->bufpos = 0;

	src->jpegbuf = malloc(JPEGBUFSIZE);

	src->pub.bytes_in_buffer = 0;
	src->pub.init_source = jpeglib_source_init;
	src->pub.fill_input_buffer = jpeglib_source_fill;
	src->pub.skip_input_data = jpeglib_source_slip;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = jpeglib_source_destroy;
	
	cinfo->src = (struct jpeg_source_mgr *)src;
}


int ofsy_image_update_from_header(sp_image *image) {
        struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	osfy_jpeglib_source(&cinfo, image->data->ptr, image->data->len);
	jpeg_read_header(&cinfo, 1);

	DSFYDEBUG("%dx%d (pitch %d, colorspace %d)\n", cinfo.image_width, cinfo.image_height, cinfo.num_components, cinfo.out_color_space);
	image->width = cinfo.image_width;
	image->height = cinfo.image_height;
	image->format = SP_IMAGE_FORMAT_RGB;

        cinfo.out_color_space = JCS_RGB;
        cinfo.buffered_image = TRUE;
        jpeg_start_decompress(&cinfo);

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

	return 0;
}


struct buf *ofsy_image_decompress(sp_image *image, int *pitch) {
        struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	struct buf *data;
	int stride;
	JSAMPARRAY scanline;

	if((data = buf_new()) == NULL)
		return NULL;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	osfy_jpeglib_source(&cinfo, image->data->ptr, image->data->len);
	jpeg_read_header(&cinfo, 1);

	DSFYDEBUG("%dx%d (pitch %d, colorspace %d)\n", cinfo.image_width, cinfo.image_height, cinfo.num_components, cinfo.out_color_space);
	image->width = cinfo.image_width;
	image->height = cinfo.image_height;
	image->format = SP_IMAGE_FORMAT_RGB;

	*pitch = 3;
        cinfo.out_color_space = JCS_RGB;
        cinfo.buffered_image = TRUE;
        jpeg_start_decompress(&cinfo);

	stride = cinfo.output_width * *pitch;
	scanline = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, stride, 1);
	while(cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines(&cinfo, scanline, 1);
		buf_append_data(data, scanline, stride);
	}

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

	return data;
}
