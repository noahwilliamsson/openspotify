#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <spotify/api.h>

#include "commands.h"
#include "debug.h"
#include "image.h"
#include "hashtable.h"
#include "request.h"
#include "sp_opaque.h"
#include "util.h"


static int osfy_image_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);


SP_LIBEXPORT(sp_image *) sp_image_create(sp_session *session, const byte image_id[20]) {
	sp_image *image;
	void **container;
	struct image_ctx *image_ctx;
	

	image = (sp_image *)hashtable_find(session->hashtable_images, image_id);
	if(image) {
		/* FIXME: What about any possible callbacks? */
		return image;
	}

	image = malloc(sizeof(sp_image));

	memcpy(image->id, image_id, sizeof(image->id));
	image->format = -1;
	image->width = -1;
	image->height = -1;
	image->data = NULL;

	image->callback = NULL;
	image->userdata = NULL;

	image->is_loaded = 0;
	image->ref_count = 1;

	image->hashtable = session->hashtable_images;
	hashtable_insert(image->hashtable, image->id, image);

	
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
	image->callback = userdata;

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
	DSFYDEBUG("Not yet implemented\n");

	return NULL;
}


SP_LIBEXPORT(void) sp_image_unlock_pixels(sp_image *image) {
	DSFYDEBUG("Not yet implemented\n");
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
	
	req->next_timeout = get_millisecs() + IMAGE_RETRY_TIMEOUT;
	
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
			image_ctx->image->is_loaded = 1;
			image_ctx->image->error = SP_ERROR_OK;

			DSFYDEBUG("Got all data, returning the image as result\n");
			request_set_result(image_ctx->session, image_ctx->req, SP_ERROR_OK, image_ctx->image);
		
			{
				FILE *fd;
				
				fd = fopen("image.jpg", "w");
				if(fd) {
					fwrite(image_ctx->image->data->ptr, image_ctx->image->data->len, 1, fd);
					fclose(fd);
				}
			}
			
			free(image_ctx);
			break;
			
		default:
			break;
	}
	
	return 0;
}
