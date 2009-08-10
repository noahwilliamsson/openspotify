#include <stdlib.h>
#include <string.h>

#include <spotify/api.h>

#include "debug.h"
#include "hashtable.h"
#include "request.h"
#include "sp_opaque.h"


SP_LIBEXPORT(sp_image *) sp_image_create(sp_session *session, const byte image_id[20]) {
	sp_image *image;
	void **container;

	image = (sp_image *)hashtable_find(session->hashtable_images, image_id);
	if(image)
		return image;

	image = malloc(sizeof(sp_image));

	memcpy(image->id, image_id, sizeof(image->id));
	image->format = -1;
	image->data = NULL;
	image->width = -1;
	image->height = -1;

	image->callback = NULL;
	image->userdata = NULL;

	image->is_loaded = 0;
	image->ref_count = 0;

	image->hashtable = session->hashtable_images;
	hashtable_insert(image->hashtable, image->id, image);

        container = (void **)malloc(sizeof(void *));
        *container = image;
        request_post(session, REQ_TYPE_IMAGE, container);

	return image;
}


SP_LIBEXPORT(void) sp_image_add_load_callback(sp_image *image, image_loaded_cb *callback, void *userdata) {
	/* FIXME: Support multiple callbacks */
	image->callback = callback;
	image->callback = userdata;
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

	if(image->ref_count)
		image->ref_count--;

	if(image->ref_count)
		return;

	/* FIXME: Free stuff */

	if(image->data)
		free(image->data);

	free(image);
}
