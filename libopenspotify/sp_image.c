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


static int osfy_image_callback(CHANNEL *ch, unsigned char *payload, unsigned short len);


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
	image->format = SP_IMAGE_FORMAT_UNKNOWN;
	image->data = NULL;

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


SP_LIBEXPORT(sp_imageformat) sp_image_format(sp_image *image) {

	return image->format;
}


SP_LIBEXPORT(const void *) sp_image_data(sp_image *image, size_t *data_size) {

	*data_size = (size_t)image->data->len;

	return image->data->ptr;
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

	/*
	 * Prevent request from happening again.
	 * If there's an error the channel callback will reset the timeout
	 *
	 */
	req->next_timeout = INT_MAX;
	req->state = REQ_STATE_RUNNING;

	image_ctx->req = req;

	{
		char buf[41];
		hex_bytes_to_ascii(image_ctx->image->id, buf, 20);
		DSFYDEBUG("Sending request for image '%s'\n", buf);
	}

	assert(image_ctx->image->data == NULL);
	image_ctx->image->data = buf_new();

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

			/* Reset timeout so the request can be retried */
			image_ctx->req->next_timeout = get_millisecs() + IMAGE_RETRY_TIMEOUT*1000;

			break;

		case CHANNEL_END:
			/* We simply assume we're always getting a JPEG image back */
			image_ctx->image->format = SP_IMAGE_FORMAT_JPEG;
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
